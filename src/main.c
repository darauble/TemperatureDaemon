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

#include <pthread.h>

#include "onewire.h"
#include "ow_driver_linux_usart.h"
#include "dallas.h"

#include "temp_types.h"

#define V_MAJOR 0
#define V_MINOR 1

#define OUTPUT_NAME_LEN 64

#define DEF_OUTPUT_FILE "/tmp/temp_daemon_%d.%s"
#define DEF_OUTPUT_FORMAT "tsv"

static int opt_daemon = 0;
static int opt_verbose = 0;
static int opt_full_scratchpad = 0;
static long int opt_address_query_period = 300; // How often to retrieve addresses of One Wire devices
static long int opt_read_period = 60; // How often to read temperatures from Dallas devices
static char *opt_output_format = DEF_OUTPUT_FORMAT;
static char output_file[OUTPUT_NAME_LEN];

static wire_t *wires = NULL;
static int wire_count = 0;
static int wire_max_count = 0;

// static thermometer_t *thermometers = NULL;
// static int thermo_count = 0;
// static int thermo_max_count = 0;

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

    printf("UART Temperature Daemon is starting up...\n");

    static int print_version = 0;

    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"version",      no_argument,       &print_version, 1},
        /* These options don’t set a flag.
            We distinguish them by their indices. */
        {"daemon",          no_argument,       0, 'D'},
        {"full_scratchpad", no_argument,       0, 'F'},
        {"device",          required_argument, 0, 'd'},
        {"output",          required_argument, 0, 'o'},
        {"query_period",    required_argument, 0, 'q'},
        {"read_period",     required_argument, 0, 'r'},
        {"verbose",         no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt_idx = 0;
    int c = 0;

    wires = malloc(WIRE_COUNT_STEP * sizeof(wire_t));

    if (wires == NULL) {
        perror("Cannot allocate memory for USART devices!\n");
    }

    wire_max_count = WIRE_COUNT_STEP;

    while(1) {

        c = getopt_long(argc, argv, "DFd:o:q:r:v", long_options, &opt_idx);

        if (c < 0) {
            break;
        }

        switch(c) {
            case 'D':
                // printf("Daemon mode set\n");
                opt_daemon = 1;
            break;

            case 'F':
                opt_full_scratchpad = 1;
            break;

            case 'd':
                // printf("Device provided: %s\n", optarg);
                
                if (wire_count >= wire_max_count) {
                    // printf("Expanding memory for devices\n");
                    wires = realloc(wires, (wire_max_count + WIRE_COUNT_STEP) * sizeof(wire_t));
                    
                    if (wires == NULL) {
                        perror("Cannot expand memory for USART devices!\n");
                    }

                    wire_max_count += WIRE_COUNT_STEP;
                }

                wires[wire_count].device = optarg;
                wires[wire_count].thermometers = NULL;

                wire_count++;
            break;

            case 'o':
                // printf("Output file provided: %s\n", optarg);
                strncpy(output_file, optarg, OUTPUT_NAME_LEN);
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
                printf("Shit! %d\n", c);
            break;
        }
    }

    if (print_version) {
        printf("UART Temperature Daemon v%d.%d\n", V_MAJOR, V_MINOR);
        return_main = 0;
        goto EXIT_MAIN;
    }

    if (opt_daemon) {
        return_main = create_daemon();
        
        if (return_main < 0) {
            goto EXIT_MAIN;
        }
    }

    if (strlen(output_file) == 0) {
        snprintf(output_file, OUTPUT_NAME_LEN, DEF_OUTPUT_FILE, getpid(), opt_output_format);
    }

    if (opt_verbose) {
        printf("Output file name: %s\n", output_file);

        printf("USART devices:\n");
        
        for (int i = 0; i < wire_count; i++) {
            printf("\t%s\n", wires[i].device);
        }
    }

    for (int i = 0; i < wire_count; i++) {
        wires[i].thermometers = malloc(THERMO_COUNT_STEP * sizeof(thermometer_t));
        wires[i].thermo_count = 0;
        wires[i].thermo_max = THERMO_COUNT_STEP;

        if (wires[i].thermometers == NULL) {
            perror("Could not allocate memory for thermometers\n");
            return_main = -1;
            goto EXIT_MAIN;
        }
    }

    for (int i = 0; i < wire_count; i++) {
        pthread_create(&wires[i].tid, NULL, temp_thread, (void *) &wires[i]);
    }

    for (int i = 0; i < wire_count; i++) {
        pthread_join(wires[i].tid, NULL);
    }


EXIT_MAIN:
    printf("Exit process\n");

    release_wires();

    // if (thermometers) {
    //     free(thermometers);
    // }

    if (wires) {
        for (int i = 0; i < wire_count; i++) {
            if (wires[i].thermometers) {
                free(wires[i].thermometers);
            }
        }
        free(wires);
    }

    unlink(output_file);

    return return_main;
}

void *temp_thread(void *wire_v)
{
    wire_t *wire = (wire_t *) wire_v;

    if (opt_verbose) {
        printf("Starting thread for device %s\n", wire->device);
    }

    init_wire(wire);

    int collect_status = collect_thermometers(wire);

    if (collect_status != 0) {
        wire->tret = -1;
        pthread_exit(&wire->tret);
    }

    read_temperatures(wire);

    wire->tret = 0;
    pthread_exit(&wire->tret);

    return NULL;
}

static int init_wire(wire_t *wire)
{
    int drv_status;

    drv_status = init_driver_linux_usart(&wire->driver, wire->device);
    
    if (drv_status != OW_OK) {
        printf("Failed to init driver for %s\n", wire->device);
    }

    owu_init(&wire->onewire, wire->driver);

    if (drv_status != OW_OK) {
        // Clear all drivers
        release_driver(&wire->driver);
        
        return -1;
    }

    return 0;
}

static void release_wires()
{
    for (int i = 0; i < wire_count; i++) {
        if (wires[i].driver != NULL) {
            release_driver(&wires[i].driver);
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

            wire->thermometers[i].status = 1;
        } else {
            printf("Error reading thermometer ");
            print_address(wire->thermometers[i].address);
            printf("\n");

            wire->thermometers[i].status = 0;

            ret_val = -2;
        }
    }

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
