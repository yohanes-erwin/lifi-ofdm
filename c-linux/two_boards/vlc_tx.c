#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "vlc_payload.h"

// Not used. Use CLI argument.
// #define NUM_DATA    100000

int NUM_DATA = 0;
static volatile uint32_t *vlctx_p;

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
        printf("Error: One argument expected (number of OFDM symbol).\n");
        return -1;
    }

    // *** Handle to physical memory ***
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
    {
        perror("Couldn't open the /dev/mem");
    }

    // *** Map a page of memory to VLC TX ***
    vlctx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0x40000000);

    // *** 16-QAM modulation and guard interval ***
    *(vlctx_p+0) = 0x122;

    // *** Send data ***
    for (int i = 0; i < NUM_DATA; i++)
    {
        // printf("Tx data: %d | \t", i);
        for (int j = 0; j < 4; j++)
        {
            // Write data to VLC TX register
            *(vlctx_p+4+j) = vlc_data[i*4+j];
            // Wait until busy flag is zero
            while ((*(vlctx_p+0) & (1 << 10)));
            // printf("0x%08X ", vlc_data[i*4+j]);
        }
        // printf("\n");
        // Wait
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = 37500;
        nanosleep(&tim, NULL);
    }
    printf("%d OFDM symbols transmitted\n", NUM_DATA);
}
