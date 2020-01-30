#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dallas.h"
#include "temp_types.h"

#define DEVICE_HEADER "NUM\tDEVICE\tSTATUS\tTHERMO_COUNT\n"
#define THERMO_HEADER "\nNUM\tDEVICE_NUM\tADDRESS\tSCRATCHPAD\tTEMPERATURE\n"
#define BUF_SIZE 88
#define FNAME_SIZE 128

int out_tsv(char *file_name, wire_t *wires, int wire_count)
{
    char output[BUF_SIZE];
    char tmp_name[FNAME_SIZE];
    int psize;

    snprintf(tmp_name, FNAME_SIZE, "%s.tmp", file_name);

    int f = open(tmp_name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

    if (f == -1) {
        perror("Error creating output file\n");
        return -1;
    }

    ssize_t w = write(f, DEVICE_HEADER, strlen(DEVICE_HEADER));

    if (w == -1) {
        printf("Error writing output file device header\n");
        close(f);
        return -1;
    }

    for (int i = 0; i < wire_count; i++) {
        psize = snprintf(output, BUF_SIZE, "%d\t%s\t%d\t%d\n",
            i, wires[i].device, wires[i].status, wires[i].thermo_count
        );
        w = write(f, output, psize);

        if (w == -1) {
            printf("Error writing device line\n");
            close(f);
            return -2;
        }
    }

    w = write(f, THERMO_HEADER, strlen(THERMO_HEADER));

    if (w == -1) {
        printf("Error writing output file thermometer header\n");
        close(f);
        return -3;
    }

    int t = 0;

    for (int i = 0; i < wire_count; i++) {
        for (int j = 0; j < wires[i].thermo_count; j++, t++) {
            uint8_t *addr = wires[i].thermometers[j].address;
            uint8_t *scr = wires[i].thermometers[j].scratchpad;

            psize = snprintf(output, BUF_SIZE,
                
                "%d\t%d\t"
                "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\t"
                "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\t"
                "%.4f\n",

                t, i,
                addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7],
                scr[SCR_L], scr[SCR_H], scr[SCR_HI_ALARM], scr[SCR_LO_ALARM], scr[SCR_CFG],
                scr[SCR_FFH], scr[SCR_RESERVED], scr[SCR_10H], scr[SCR_CRC],
                wires[i].thermometers[j].temperature
            );

            w = write(f, output, psize);

            if (w == -1) {
                printf("Error writing thermometer line\n");
                close(f);
                return -4;
            }
        }
    }
    
    close(f);

    rename(tmp_name, file_name);

    return 0;
}
