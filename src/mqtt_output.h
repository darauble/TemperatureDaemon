#include "temp_types.h"

void mqtt_open(char *server, int port, char *topic_base);

void mqtt_send(wire_t *wires, int wire_count);

void mqtt_close();
