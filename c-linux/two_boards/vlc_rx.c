#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "vlc_payload.h"

// Not used. Use CLI argument.
// #define NUM_DATA    100000

static volatile uint32_t *vlcrx_p;

int NUM_DATA = 0;
uint32_t rx_data[400000];
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
		printf("Error: One argument expected (number of OFDM symbol).\n");
		return -1;
	}

	// *** Handle to physical memory ***
	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
	{
		perror("Couldn't open the /dev/mem");
	}

	// *** Map a page of memory to LiFi TX and RX ***
	vlcrx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0x42000000);

    // *** 16-QAM demodulation ***
    *(vlcrx_p+0) = 0x2;

	// *** Receive data ***
    for (int i = 0; i < NUM_DATA; i++)
    {
        // Wait until ready flag is one
        while (!(*(vlcrx_p+0) & (1 << 2)));
		// printf("Rx data: %d | \t", i);
        for (int j = 0; j < 4; j++)
		{
            // Read data from VLC RX register
			rx_data[i*4+j] = *(vlcrx_p+4+j);
            // printf("0x%08X ", rx_data[i*4+j]);
		}
        // printf("\n");
    }
    printf("%d OFDM symbols received\n", NUM_DATA);

	// *** Calculate BER ***
	for (int i = 0; i < NUM_DATA; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			for (int k = 0; k < 32; k++)
			{
				if ((rx_data[i*4+j] & (1 << k)) != (vlc_data[i*4+j] & (1 << k)))
				{
					total_bit_error++;
				}
			}
		}
	}
	printf("Total bit error: %d\n", total_bit_error);
	
    return 0;
}
