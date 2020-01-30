#ifndef __TEMP_OUTPUT_H__
#define __TEMP_OUTPUT_H__

#include "temp_types.h"

int out_tsv(char *file_name, wire_t *wires, int wire_count);

int out_json(char *file_name, wire_t *wires, int wire_count);

#endif /* __TEMP_OUTPUT_H__ */