/*
 * The source of the UART Temperature Daemon designed around OneWire
 * "driver" of Linux USART adapter (e.g. FT232 or similar).
 *
 * main.c
 *
 *  Created on: Jan 23, 2020
 *      Author: Darau, blė
 *
 *  Credits: Chi Zhang aka dword1511
 *
 *  This file is a part of personal use libraries developed to be used
 *  on various microcontrollers and Linux devices.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysinfo.h>

#include <pthread.h>

#include "onewire.h"
#include "ow_driver_linux_usart.h"
#include "dallas.h"

#include "temp_types.h"
#include "temp_output.h"
#include "mqtt_output.h"

#define V_MAJOR 0
#define V_MINOR 1

static int opt_daemon = 0;
static int opt_verbose = 0;
static int opt_full_scratchpad = 0;
static long int opt_address_query_period = 300; // How often to retrieve addresses of One Wire devices
static long int opt_read_period = 60; // How often to read temperatures from Dallas devices
// static char *opt_output_format = DEF_OUTPUT_FORMAT;

static int opt_tsv = 0;
static char *output_tsv = NULL;

static int opt_json = 0;
static char *output_json = NULL;

static int opt_mqtt = 0;
static int opt_mqtt_dummy = 0;
static char *mqtt_server = NULL;
static int mqtt_port = 1883;
static char *mqtt_topic = "darauble/temp_daemon";

static wire_t *wires = NULL;
static int wire_count = 0;
static int wire_max_count = 0;

static long last_uptime = 0;
static long current_uptime = 0;

// static thermometer_t *thermometers = NULL;
// static int thermo_count = 0;
// static int thermo_max_count = 0;

void usage();

static int init_wire(wire_t *);
static void release_wires();
static int collect_thermometers(wire_t *);
static int read_temperatures(wire_t *);
static int create_daemon();
void *temp_thread(void *);

static void print_address(uint8_t *);

int main(int argc, char **argv)
{
    __label__ EXIT_MAIN;
    int return_main = 0;

    static int print_version = 0;

    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"version",      no_argument,       &print_version, 1},
        {"tsv",          required_argument, &opt_tsv, 1},
        {"json",         required_argument, &opt_json, 1},
        {"mqtt_server",  required_argument, &opt_mqtt, 1},
        {"mqtt_port",  required_argument, &opt_mqtt_dummy, 1},
        {"mqtt_topic",  required_argument, &opt_mqtt_dummy, 1},
        /* These options don’t set a flag.
            We distinguish them by their indices. */
        {"daemon",          no_argument,       0, 'D'},
        {"full_scratchpad", no_argument,       0, 'F'},
        {"device",          required_argument, 0, 'd'},
        {"help",            no_argument,       0, 'h'},
        {"query_period",    required_argument, 0, 'q'},
        {"read_period",     required_argument, 0, 'r'},
        {"verbose",         no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt_idx = 0;
    int c = 0;

    wires = malloc(WIRE_COUNT_STEP * sizeof(wire_t));

    if (wires == NULL) {
        fprintf(stderr, "Cannot allocate memory for USART devices!\n");
    }

    wire_max_count = WIRE_COUNT_STEP;

    while(1) {

        c = getopt_long(argc, argv, "DFd:hq:r:v", long_options, &opt_idx);

        if (c < 0) {
            break;
        }

        switch(c) {
            case 'D':
                opt_daemon = 1;
            break;

            case 'F':
                opt_full_scratchpad = 1;
            break;

            case 'd':
                if (wire_count >= wire_max_count) {
                    wires = realloc(wires, (wire_max_count + WIRE_COUNT_STEP) * sizeof(wire_t));
                    
                    if (wires == NULL) {
                        fprintf(stderr, "Cannot expand memory for USART devices!\n");
                    }

                    wire_max_count += WIRE_COUNT_STEP;
                }

                wires[wire_count].last_query = 0;
                wires[wire_count].driver = NULL;
                wires[wire_count].status = TEMP_STATUS_FAIL; // Uninitialized wire
                wires[wire_count].device = optarg;
                wires[wire_count].thermometers = NULL;

                wire_count++;
            break;

            case 'h':
                usage();
                goto EXIT_MAIN;
            break;

            case 'q':
                opt_address_query_period = strtol(optarg, NULL, 10);
            break;

            case 'r':
                opt_read_period = strtol(optarg, NULL, 10);
            break;

            case 'v':
                printf("Verbose mode on\n");
                opt_verbose = 1;
            break;

            case '?':
                printf("Shit?\n");
                return_main = -1;
                goto EXIT_MAIN;
            break;

            default:
                // printf("Shit! %d, %d\n", c, opt_idx);
                switch(opt_idx) {
                    case 1:
                        /* TSV output defined */
                        // printf("TSV file defined: %s\n", optarg);
                        output_tsv = optarg;
                    break;

                    case 2:
                        /* JSON output defined */
                        // printf("JSON file defined: %s\n", optarg);
                        output_json = optarg;
                    break;

                    case 3:
                        /* MQTT server set */
                        // printf("MQTT server defined: %s\n", optarg);
                        mqtt_server = optarg;
                    break;

                    case 4:
                        /* MQTT port defined */
                        mqtt_port = strtol(optarg, NULL, 10);
                        // printf("MQTT port defined: %d\n", mqtt_port);
                    break;

                    case 5:
                        /* MQTT topic set */
                        // printf("MQTT topic defined: %s\n", optarg);
                        mqtt_topic = optarg;
                    break;
                }
            break;
        }
    }

    if (print_version) {
        printf("USART Temperature Daemon v%d.%d\n", V_MAJOR, V_MINOR);
        return_main = 0;
        goto EXIT_MAIN;
    }

    if (opt_daemon) {
        return_main = create_daemon();
        
        if (return_main < 0) {
            goto EXIT_MAIN;
        }
    }

    if (opt_verbose) {
        printf("USART Temperature Daemon is starting up...\n");

        if (output_tsv != NULL) {
            printf("Write output to %s\n", output_tsv);
        }

        if (output_json != NULL) {
            printf("Write output to %s\n", output_json);
        }

        if (mqtt_server != NULL) {
            printf("Send output to MQTT: %s:%d @ %s\n", mqtt_server, mqtt_port, mqtt_topic);
        }

        printf("USART devices:\n");
        
        for (int i = 0; i < wire_count; i++) {
            printf("  %s\n", wires[i].device);
        }
    }

    for (int i = 0; i < wire_count; i++) {
        wires[i].thermometers = malloc(THERMO_COUNT_STEP * sizeof(thermometer_t));
        wires[i].thermo_count = 0;
        wires[i].thermo_max = THERMO_COUNT_STEP;

        if (wires[i].thermometers == NULL) {
            fprintf(stderr, "Could not allocate memory for thermometers\n");
            return_main = -1;
            goto EXIT_MAIN;
        }
    }

    if (mqtt_server != NULL) {
        mqtt_open(mqtt_server, mqtt_port, mqtt_topic);
    }

    while (1) {
        struct sysinfo s_info;
        int err = sysinfo(&s_info);
        if (err != 0) {
            fprintf(stderr, "Cannot read uptime, daemon cannot run, exiting.\n");
            goto EXIT_MAIN;
        }

        current_uptime = s_info.uptime;
    

        if (current_uptime - last_uptime >= opt_read_period) {
            for (int i = 0; i < wire_count; i++) {
                pthread_create(&wires[i].tid, NULL, temp_thread, (void *) &wires[i]);
            }

            for (int i = 0; i < wire_count; i++) {
                pthread_join(wires[i].tid, NULL);
            }

            last_uptime = current_uptime;

            if (opt_tsv) {
                out_tsv(output_tsv, wires, wire_count);
            }

            if (opt_json) {
                out_json(output_json, wires, wire_count);
            }

            if (mqtt_server != NULL) {
                mqtt_send(wires, wire_count);
            }

            printf("[%ld] Temperatures read.\n", current_uptime);
        }

        sleep(1);
    }

EXIT_MAIN:
    if (opt_verbose) {
        printf("Exit temp_daemon\n");
    }

    release_wires();

    if (wires) {
        for (int i = 0; i < wire_count; i++) {
            if (wires[i].thermometers) {
                free(wires[i].thermometers);
            }
        }
        free(wires);
    }

    // unlink(output_file);

    return return_main;
}

void *temp_thread(void *wire_v)
{
    __label__ EXIT_THREAD;

    wire_t *wire = (wire_t *) wire_v;

    if (opt_verbose) {
        printf("Starting thread for device %s\n", wire->device);
    }

    int collect_status = 0;
    
    if (wire->status == TEMP_STATUS_OK) {
        if (opt_address_query_period > 0 && (current_uptime - wire->last_query >= opt_address_query_period)) {
            collect_status = collect_thermometers(wire);
            wire->last_query = current_uptime;
        }
    } else {
        collect_status = init_wire(wire);
        
        if (collect_status != 0) {
            goto EXIT_THREAD;
        }

        collect_status = collect_thermometers(wire);
        wire->last_query = current_uptime;
    }

    if (collect_status != 0) {
        goto EXIT_THREAD;
    }

    int read_status = read_temperatures(wire);

    if (read_status != 0) {
        goto EXIT_THREAD;
    }

    wire->tret = 0;

    pthread_exit(&wire->tret);

    return NULL;

EXIT_THREAD:
    fprintf(stderr, "[%ld] Device %s failed, will be reinitialized.\n", current_uptime, wire->device);

    if (wire->driver != NULL) {
        release_driver(&wire->driver);
        wire->driver = NULL;
    }
    
    wire->status = TEMP_STATUS_FAIL;
    wire->tret = -1;

    pthread_exit(&wire->tret);

    return NULL;
}

static int init_wire(wire_t *wire)
{
    int drv_status;

    drv_status = init_driver_linux_usart(&wire->driver, wire->device);
    
    if (drv_status != OW_OK) {
        printf("Failed to init driver for %s\n", wire->device);
        wire->driver = NULL;
        return -2;
    }

    owu_init(&wire->onewire, wire->driver);

    if (drv_status != OW_OK) {
        // Clear all drivers
        release_driver(&wire->driver);
        wire->driver = NULL;
        
        return -1;
    }

    wire->status = TEMP_STATUS_OK;

    return 0;
}

static void release_wires()
{
    for (int i = 0; i < wire_count; i++) {
        if (wires[i].driver != NULL) {
            release_driver(&wires[i].driver);
            wires[i].driver = NULL;
            wires[i].status = TEMP_STATUS_FAIL;
        }
    }
}

static int collect_thermometers(wire_t *wire) {
    wire->thermo_count = 0;

    if (opt_verbose) {
        printf("Startng search of thermometers...\n");
    }

    owu_reset_search(&wire->onewire);

    if (wire->thermo_count >= wire->thermo_max) {
        if (opt_verbose) {
            printf("Expanding memory for more thermometers\n");
        }

        wire->thermometers = realloc(wire->thermometers, (wire->thermo_max + THERMO_COUNT_STEP) * sizeof(thermometer_t));

        if (wire->thermometers == NULL) {
            return -1;
        }
    }

    while(owu_search(&wire->onewire, wire->thermometers[wire->thermo_count].address)) {
        if (opt_verbose) {
            printf("  Found ");
            print_address(wire->thermometers[wire->thermo_count].address);
            printf(" @ %s\n", wire->device);
        }

        wire->thermo_count++;
    }

    if (opt_verbose) {
        printf("... search done.\n");
    }

    if (wire->thermo_count == 0) {
        fprintf(stderr, "[%ld] Could not find thermometers on device %s\n", current_uptime, wire->device);
        
        return -2;
    }

    printf("[%ld] Collected %d thermometers on device %s\n", current_uptime, wire->thermo_count, wire->device);

    return 0;
}

static int read_temperatures(wire_t *wire)
{
    if (opt_verbose) {
        printf("Start conversion @ %s\n", wire->device);
    }
    int convert_status = ds_convert_all(&wire->onewire);

    if (convert_status != OW_OK) {
        printf("Convert: no devices on %s\n", wires->device);
        return -1;
    }

    sleep(1);

    int ret_val = 0;

    for (int i = 0; i < wire->thermo_count; i++) {
        int read_status;

        if (opt_full_scratchpad) {
            read_status = ds_read_scratchpad(
                &wire->onewire, 
                wire->thermometers[i].address,
                wire->thermometers[i].scratchpad
            );
        } else {
            read_status = ds_read_temp_only(
                &wire->onewire, 
                wire->thermometers[i].address,
                wire->thermometers[i].scratchpad
            );
        }

        if (read_status == OW_OK) {
            wire->thermometers[i].temperature = ds_get_temp_c(wire->thermometers[i].scratchpad);

            if (opt_verbose) {
                printf("Temperature @ ");
                print_address(wire->thermometers[i].address);
                printf(": %.4f\n", wire->thermometers[i].temperature);
            }

            wire->thermometers[i].status = TEMP_STATUS_OK;
        } else {
            printf("Error reading thermometer ");
            print_address(wire->thermometers[i].address);
            printf("\n");

            wire->thermometers[i].status = TEMP_STATUS_FAIL;

            ret_val = -2;
        }
    }

    printf("[%ld] Read %d thermometers on device %s\n", current_uptime, wire->thermo_count, wire->device);

    return ret_val;
}

static void print_address(uint8_t *addr) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
        addr[0], addr[1], addr[2], addr[3],
        addr[4], addr[5], addr[6], addr[7]
    );
}

static int create_daemon()
{
    pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid > 0) {
        return -3;
    }

    if (setsid() < 0) {
        return -2;
    }

    return 0;
}

void usage()
{
    printf(
        "Usage: temp_daemon -d <USART device> [options]\n"
        "\n"
        "Main options:\n"
        "  -D, --daemon                      Run application in daemon mode.\n"
        "  -d <device>, --device=<device>    Set at least one (or more) devices to read DALLAS temperatures through.\n"
        "                                    E.g. temp_daemon -d /dev/ttyUSB0\n"
        "                                    E.g. temp_daemon -d /dev/ttyUSB0 -d /dev/ttyACM1\n"
        "  -q <sec>, --query_period=<sec>    Set period in seconds to search for DALLAS temperature devices.\n"
        "                                    Set to 0 (zero) to search for devices only once on startup\n"
        "                                    Default period is 300 s (5 min.).\n"
        "  -r <sec>, --read_period=<sec>     Set period in seconds to read temperature and print output.\n"
        "  -F, --full_scratchpad             Read full scratchpad, all 9 bytes. By default only 2 first bytes are read,\n"
        "                                    as that's enough to convert the temperature. A bit faster.\n"
        "\n"
        "Output options, can be used simultaneously:\n"
        "  --tsv=<file>                      Write output to TSV file.\n"
        "  --json=<file>                     Write output to JSON file.\n"
        "  --mqtt_server=<server>            Send output to MQTT server.\n"
        "  --mqtt_port=<port>                Set MQTT server's port. Default 1883.\n"
        "  --mqtt_topic=<topic>              Set parent MQTT topic. Default \"darauble/temp_daemon\"\n"
        "\n"
        "Other options:\n"
        "  -v, --verbose                     Print verbose output of daemon's actions.\n"
        "  -h, --help                        Print this usage message and exit.\n"
        "  --version                         Print application's version and exit.\n"
        "\n"
    );
}
