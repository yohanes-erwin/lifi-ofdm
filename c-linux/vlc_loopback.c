// Author: Erwin Ouyang
// Date  : 25 Sep 2018
// Update: 25 Sep 2018 - Adapted from Fuad Ismail's code
//         26 Sep 2018 - Porting to RedPitaya
//         05 Feb 2019 - 50 MHz, one board loopback, GitHub first commit, from lifi_txrx_no_dbg.c 

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

// Not used. Use CLI argument.
//#define MOD_TYPE	2

//#if 	MOD_TYPE == 0
//#define PAYLOAD_BIT			31
//#elif 	MOD_TYPE == 1
//#define PAYLOAD_BIT			62
//#elif	MOD_TYPE == 2
//#define PAYLOAD_BIT			124
//#endif

//#if 	MOD_TYPE == 0
//#define NUM_OF_WORD			1
//#elif 	MOD_TYPE == 1
//#define NUM_OF_WORD			2
//#elif	MOD_TYPE == 2
//#define NUM_OF_WORD			4
//#endif

#define AXI_LIFI_TX 0x41200000
#define AXI_LIFI_RX 0x41210000

static volatile uint32_t *lifi_tx_p;
static volatile uint32_t *lifi_rx_p;

int fd;
uint8_t MOD_TYPE = 0;
uint32_t NUM_OF_SYM = 0;
uint16_t PAYLOAD_BIT = 0;
uint16_t NUM_OF_WORD = 0;
uint32_t tx_buffer_p[4], rx_buffer_p[4];
uint32_t total_bit_error = 0;

void send_ofdm_random(void);

int main(int argc, char *argv[])
{
	// *** Get encoding and modulation type ***
	if (argc == 3)
	{
		MOD_TYPE = atoi(argv[1]);
		NUM_OF_SYM = atoi(argv[2]);
		PAYLOAD_BIT = ((MOD_TYPE == 0) ? 31 : ((MOD_TYPE == 1) ? 62 : ((MOD_TYPE == 2) ? 124 : 31)));
		NUM_OF_WORD = ((MOD_TYPE == 0) ? 1 : ((MOD_TYPE == 1) ? 2 : ((MOD_TYPE == 2) ? 4 : 1)));
	}	
	else if (argc > 3)
	{
		printf("Error: Too many arguments supplied.\n");
		return -1;
	}
	else
	{
		printf("Error: Two argument expected (modulation mode and number of symbols).\n");
		return -1;
	}
	
	// *** Handle to physical memory ***
	if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
	{
		perror("Couldn't open the /dev/mem");
	}

	// *** Map a page of memory to LiFi TX and RX ***
	lifi_tx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, AXI_LIFI_TX);
	lifi_rx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, AXI_LIFI_RX);
	
	// *** Configure LiFi TX and RX ***
	if (MOD_TYPE == 0)
	{
		*(lifi_tx_p+0) = 0x120;
		*(lifi_rx_p+0) = 0x0;
	}
	else if (MOD_TYPE == 1)
	{
		*(lifi_tx_p+0) = 0x121;
		*(lifi_rx_p+0) = 0x1;
	}
	else if (MOD_TYPE == 2)
	{
		*(lifi_tx_p+0) = 0x122;
		*(lifi_rx_p+0) = 0x2;
	}

	// *** Random seed ***
//	srand(129471236);
//	srand(239419123);
//	srand(328142135);
//	srand(499342331);
//	srand(518924233);
//	srand(634849284);
//	srand(736723672);
//	srand(892421432);
//	srand(992319912);
	srand(1038295326);
	
	// *** Send and receive symbol ***
	uint32_t cnt_symbol = 0;
	if (MOD_TYPE == 0)
	{
		printf("============================ BPSK ============================\n");
		for (int i = 0; i < NUM_OF_SYM; i++)
		{
			send_ofdm_random();
			cnt_symbol++;
			if ((NUM_OF_SYM >= 10000000) && (cnt_symbol%1000000) == 0)
			{
				printf(".");
				fflush(stdout);
			}
		}
		if (NUM_OF_SYM >= 10000000)
			printf("\n");
	}
	else if (MOD_TYPE == 1)
	{
		printf("============================ QPSK ============================\n");
		for (int i = 0; i < NUM_OF_SYM; i++)
		{
			send_ofdm_random();
			cnt_symbol++;
			if ((NUM_OF_SYM >= 10000000) && (cnt_symbol%1000000) == 0)
			{
				printf(".");
				fflush(stdout);
			}
		}
		if (NUM_OF_SYM >= 10000000)
			printf("\n");
	}
	else if (MOD_TYPE == 2)
	{
		printf("=========================== QAM-16 ===========================\n");
		for (int i = 0; i < NUM_OF_SYM; i++)
		{
			send_ofdm_random();
			cnt_symbol++;
			if ((NUM_OF_SYM >= 10000000) && (cnt_symbol%1000000) == 0)
			{
				printf(".");
				fflush(stdout);
			}
		}
		if (NUM_OF_SYM >= 10000000)
			printf("\n");
	}
	
	// *** Print results ***
	printf("Total transmitted data: %d bit, %.1f Mbit, %.1f MByte\n", PAYLOAD_BIT*cnt_symbol,
			(double)(PAYLOAD_BIT*cnt_symbol/1024.0/1024.0),
			(double)(PAYLOAD_BIT*cnt_symbol/8/1024.0/1024.0));
	printf("Total bit error: %d\n", total_bit_error);
	double ber = total_bit_error / ((double)PAYLOAD_BIT*cnt_symbol);
	printf("BER: %.9f, %4.6e\n", ber, ber);
	printf("===============================================================\n");
	
	return 0;
}

void send_ofdm_random()
{
	// *** Generate random data ***
	if (MOD_TYPE == 0)
	{
		tx_buffer_p[0] = rand() & 0xFFFFFFFE;
		*(lifi_tx_p+4) = tx_buffer_p[0];
	}
	else if (MOD_TYPE == 1)
	{
		tx_buffer_p[0] = rand() & 0xFFFFFFFF;
		tx_buffer_p[1] = rand() & 0xFFFFFFFC;
		*(lifi_tx_p+4) = tx_buffer_p[0];
		*(lifi_tx_p+5) = tx_buffer_p[1];
	}
	else if (MOD_TYPE == 2)
	{
		tx_buffer_p[0] = rand() & 0xFFFFFFFF;
		tx_buffer_p[1] = rand() & 0xFFFFFFFF;
		tx_buffer_p[2] = rand() & 0xFFFFFFFF;
		tx_buffer_p[3] = rand() & 0xFFFFFFF0;
		*(lifi_tx_p+4) = tx_buffer_p[0];
		*(lifi_tx_p+5) = tx_buffer_p[1];
		*(lifi_tx_p+6) = tx_buffer_p[2];
		*(lifi_tx_p+7) = tx_buffer_p[3];
	}

	// Wait until not busy
	while ((*(lifi_tx_p+0) & (1 << 10)));
	
	// Wait until data ready
	while (!(*(lifi_rx_p+0) & (1 << 2)));
	
	// *** Copy from PHY RX memory ***
	if (MOD_TYPE == 0)
	{
		rx_buffer_p[0] = *(lifi_rx_p+4);
	}
	else if (MOD_TYPE == 1)
	{
		rx_buffer_p[0] = *(lifi_rx_p+4);
		rx_buffer_p[1] = *(lifi_rx_p+5);
	}
	else if (MOD_TYPE == 2)
	{
		rx_buffer_p[0] = *(lifi_rx_p+4);
		rx_buffer_p[1] = *(lifi_rx_p+5);
		rx_buffer_p[2] = *(lifi_rx_p+6);
		rx_buffer_p[3] = *(lifi_rx_p+7);
	}
	
	// *** Compare TX and RX data *** =========================================
	for (int i = 0; i < NUM_OF_WORD; i++)
	{
		for (int j = 0; j < 32; j++)
		{
			// *** If TX and RX bits are different ***
			if ((tx_buffer_p[i] & (1 << (31-j))) !=
					(rx_buffer_p[i] & (1 << (31-j))))
				total_bit_error++;
		}
	}
}