#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "irc_payload.h"

// Not used. Use CLI argument.
// #define NUM_DATA    100

static volatile uint32_t *irctx_p;
int NUM_DATA = 0;

int main(int argc, char *argv[])
{
    int fd;

   // *** Get NUM_DATA ***
    if (argc == 2)
    {
        NUM_DATA = atoi(argv[1]);
    }
    else if (argc > 2)
    {
        printf("Error: Too many arguments supplied.\n");
        return -1;
    }
    else
    {
        printf("Error: One argument expected (number of IRC data).\n");
        return -1;
    }

    // *** Handle to physical memory ***
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
    {
        perror("Couldn't open the /dev/mem");
    }

    // *** Map a page of memory to IRC TX ***
    irctx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0x43000000);

    // *** Transmit IRC data ***
    for (int i = 0; i < NUM_DATA; i++)
    {
        // Write data to IRC TX register
        *(irctx_p+1) = irc_data[i];
        // Wait until busy flag is zero
        while ((*(irctx_p+0) & (1 << 17)));
    }

    return 0;
}
