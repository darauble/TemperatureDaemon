#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "temp_types.h"

#define HEADER "DEVICE  ADDRESS SCRATCHPAD  TEMPERATURE\n"

int out_tsv(char *file_name, thermometer_t *thermometers, int thermo_count)
{
    int f = open(file_name, O_CREAT | O_WRONLY);

    if (f == -1) {
        perror("Error creating output file\n");
        return -1;
    }

    ssize_t w = write(f, HEADER, strlen(HEADER));

    if (w == -1) {
        printf("Error writing output file header\n");
        close(f);
        return -1;
    }

    

    close(f);
    return 0;
}