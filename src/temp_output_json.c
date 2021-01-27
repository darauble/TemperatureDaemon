#include <jansson.h>

#include "dallas.h"
#include "temp_types.h"

#define JDUMP_FLAGS JSON_INDENT(2) | JSON_ESCAPE_SLASH
#define BUF_SIZE 128

int out_json(char *file_name, wire_t *wires, int wire_count)
{
    char output[BUF_SIZE];

    json_t *jdev = json_array();
    json_t *jtemp = json_array();

    int t = 0;

    for (int i = 0; i < wire_count; i++) {
        json_t *jwire = json_object();

        json_object_set_new(jwire, "num", json_integer(i));
        json_object_set_new(jwire, "device", json_string(wires[i].device));
        json_object_set_new(jwire, "status", json_integer(wires[i].status));
        json_object_set_new(jwire, "thermo_count", json_integer(wires[i].thermo_count));

        for (int j = 0; j < wires[i].thermo_count; j++, t++) {
            json_t *jthermo = json_object();

            json_object_set_new(jthermo, "num", json_integer(t));
            json_object_set_new(jthermo, "device_num", json_integer(i));
            json_object_set_new(jthermo, "status", json_integer(wires[i].thermometers[j].status));

            uint8_t *addr = wires[i].thermometers[j].address;
            uint8_t *scr = wires[i].thermometers[j].scratchpad;

            snprintf(output, BUF_SIZE,
                "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7]
            );

            json_object_set_new(jthermo, "address", json_string(output));

            if (wires[i].thermometers[j].status != TEMP_STATUS_FAIL) {
                snprintf(output, BUF_SIZE,
                    "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    scr[SCR_L], scr[SCR_H], scr[SCR_HI_ALARM], scr[SCR_LO_ALARM], scr[SCR_CFG],
                    scr[SCR_FFH], scr[SCR_RESERVED], scr[SCR_10H], scr[SCR_CRC]
                );

                json_object_set_new(jthermo, "scratchpad", json_string(output));
                json_object_set_new(jthermo, "temperature", json_real(wires[i].thermometers[j].temperature));
            }

            json_array_append_new(jtemp, jthermo);
        }

        json_array_append_new(jdev, jwire);
    }

    snprintf(output, BUF_SIZE, "%s.tmp", file_name);

    json_t *jout = json_object();

    json_object_set_new(jout, "devices", jdev);
    json_object_set_new(jout, "thermometers", jtemp);

    json_dump_file(jout, output, JDUMP_FLAGS);

    json_decref(jout);

    rename(output, file_name);

    return 0;
}
