#ifndef __TEMP_TYPES_H__
#define __TEMP_TYPES_H__

#define WIRE_COUNT_STEP 5
#define THERMO_COUNT_STEP 5

#include <pthread.h>

typedef struct thermometer {
    uint8_t address[8];
    uint8_t scratchpad[__SCR_LENGTH];
    float temperature;
    int status;
} thermometer_t;


typedef struct wire {
    char *device;
    owu_struct_t onewire;
    ow_driver_ptr driver;
    int status;

    pthread_t tid;
    int tret;

    int thermo_count;
    int thermo_max;
    thermometer_t *thermometers;
} wire_t;

#endif /* __TEMP_TYPES_H__ */