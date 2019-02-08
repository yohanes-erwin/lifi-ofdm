#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "irc_payload.h"

// Not used. Use CLI argument.
// #define NUM_DATA    100

static volatile uint32_t *ircrx_p;
int NUM_DATA = 0;
uint32_t rx_data[100000];
uint32_t total_bit_error = 0;

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

    // *** Map a page of memory to IRC RX ***
    ircrx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0x41000000);

    // *** Receive IRC data ***
    for (int i = 0; i < NUM_DATA; i++)
    {
        // Wait until ready flag is one
        while (!(*(ircrx_p+0) & (1 << 16)));
        // Read data from IRC RX register
        rx_data[i] =  *(ircrx_p+1);
    }

    // *** Calculate BER ***
    for (int i = 0; i < NUM_DATA; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if ((rx_data[i] & (1 << j)) != (irc_data[i] & (1 << j)))
            {
                total_bit_error++;
            }
        }
    }
    printf("Total bit error: %d\n", total_bit_error);

    return 0;
}
