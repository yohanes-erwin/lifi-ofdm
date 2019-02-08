// *** Author: Erwin Ouyang
// *** Date  : 28 Sep 2018
// *** Note  : Adapted from Fuad Ismail's code

// ### Description #############################################################
// This code is for LiFi station
//  					 LiFi Station                      LiFi Access Point
// 	client_ethernet -> red_pi(eth0->irc) -> irc channel -> red_pi(irc->wlan0) -> wifi_router -> internet
//	client_ethernet <- red_pi(eth0<-vlc) <- vlc channel <- red_pi(vlc<-wlan0) <- wifi_router <- internet

// ### Includes ################################################################
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>

// ### Configuration ###########################################################
// This iface is connected to laptop
#define ETH_IFACE			"eth0"
// ETH iface, eth0, 192.168.3.105
#define MAC_ETHERNET_1		0x00
#define MAC_ETHERNET_2		0x26 
#define MAC_ETHERNET_3		0x32
#define MAC_ETHERNET_4		0xF0
#define MAC_ETHERNET_5		0x56
#define MAC_ETHERNET_6		0x70
// Laptop client, 192.168.3.1
#define MAC_LAPTOP_1		0x00
#define MAC_LAPTOP_2		0x30 
#define MAC_LAPTOP_3		0x67
#define MAC_LAPTOP_4		0x0B
#define MAC_LAPTOP_5		0xE0
#define MAC_LAPTOP_6		0xFD
#define IP_LAPTOP 			"192.168.3.1"

// ### Defines #################################################################
// *** PHY address ***
#define AXI_VLC_RX		0x41210000
#define AXI_IRC_TX 		0x41240000
// *** Ethernet ***
#define FRAM_SIZE		1518
// *** Ring buffer ***
#define BUFF_SIZE 		256
// *** OFDM ***
#define OFDM_WORD		4
#define OFDM_BYTE		15
#define OFDM_BIT		120
// *** Shared memory ***
#define SHM_SIZE 		9

// *** Uplink ******************************************************************
// Ring buffer size
#define BUFF_UP_SIZE 	2048

// *** Downlink ****************************************************************
// Ring buffer size
#define BUFF_DL_SIZE 	2048

#define HEADER_MISSING	1
#define ACK_FOUND		2

// ### Struct definitions ######################################################
typedef struct pseudotcp_t
{
	uint32_t saddr;
	uint32_t daddr;
	uint8_t zero;
	uint8_t protocol;
	uint16_t len;
} pseudotcp_t;

typedef struct ethfrm_t
{
	uint8_t data[FRAM_SIZE];	// Ethernet packet data
	uint16_t bytes;				// Ethernet packet length
} ethfrm_t;

typedef struct ofdmsym_t
{
	uint32_t data[OFDM_WORD];	// OFDM symbol data
	uint16_t bytes;				// Number of data bytes
} ofdmsym_t;

// ### Variables ###############################################################
// *** PHY memory map ***
int fd_mem;
static volatile uint32_t *vlc_rx_p;
static volatile uint32_t *ook_tx_p;
// *** Socket ***
int fd_sock;
struct sockaddr saddr;
int saddr_len;

// *** Uplink ******************************************************************
// *** Thread ***
pthread_t thread_recveth, thread_sendirc;
pthread_mutex_t mutex_up = PTHREAD_MUTEX_INITIALIZER;
// *** Ethernet frame fifo buffer ***
struct ethfrm_t buff_up[BUFF_UP_SIZE];
uint16_t head_up = 0, tail_up = 0;
uint8_t empty_up = 1, full_up = 0;
// ETH's MAC
uint8_t mac_ethernet[6] = 
{
	MAC_ETHERNET_1,
	MAC_ETHERNET_2,
	MAC_ETHERNET_3,
	MAC_ETHERNET_4,
	MAC_ETHERNET_5,
	MAC_ETHERNET_6
};

// *** Downlink ****************************************************************
// *** Thread ***
pthread_t thread_recvvlc, thread_sendeth;
pthread_mutex_t mutex_dl = PTHREAD_MUTEX_INITIALIZER;
// *** Ethernet frame fifo buffer ***
struct ethfrm_t buff_dl[BUFF_DL_SIZE];
uint16_t head_dl = 0, tail_dl = 0;
uint8_t empty_dl = 1, full_dl = 0;
// Laptop's MAC
uint8_t mac_laptop[6] =
{
	MAC_LAPTOP_1,
	MAC_LAPTOP_2,
	MAC_LAPTOP_3,
	MAC_LAPTOP_4,
	MAC_LAPTOP_5,
	MAC_LAPTOP_6
};
// Laptop's IP
char *ip_laptop = IP_LAPTOP;

// *** ACK *********************************************************************
uint8_t ack = 0;
uint8_t send_ack_flag = 0;
pthread_t thread_sendack;
pthread_mutex_t mutex_ack, mutex_ooktx, mutex_ackflag = 
		PTHREAD_MUTEX_INITIALIZER;
		
// ### Function prototypes #####################################################
// *** PHY layer initialization ***
void phy_init(void);

// *** Ethernet frame functions *** 
void ethfrm_set(ethfrm_t *ethfrm, uint8_t *data, uint16_t bytes);
void ethfrm_print(ethfrm_t ethfrm);
// *** Data link layer functions ***
uint8_t recv_vlc_frm(struct ethfrm_t *ethfrm);
void send_irc_frm(ethfrm_t ethfrm);
void send_ack(void);
// *** PHY layer functions ***
void recv_ofdm_sym(struct ofdmsym_t *ofdmsym);
void send_ook_sym(uint8_t data);

// *** Checksum ***
unsigned short checksum(unsigned short *buff, int _16bitword);
unsigned short checksumtcp(const char *buff, unsigned size);

// *** Uplink ******************************************************************
// *** Thread handler ***
void *recveth_handler();
void *sendirc_handler();
// *** FIFO buffer functions ***
uint8_t buff_up_push(ethfrm_t ethfrm);
uint8_t buff_up_pop(ethfrm_t *ethfrm);
void buff_up_print(void);

// *** Downlink ****************************************************************
// *** Thread handler ***
void *recvvlc_handler();
void *sendeth_handler();
// *** FIFO buffer functions ***
uint8_t buff_dl_push(ethfrm_t ethfrm);
uint8_t buff_dl_pop(ethfrm_t *ethfrm);
void buff_dl_print(void);

// *** ACK *********************************************************************
void *sendack_handler();

// ### Main ####################################################################
int main(int argc, char *argv[])
{
	// ### Set to highest priority #############################################
	setpriority(PRIO_PROCESS, 0, -20);

	// ### Disable kernel packet processing ####################################
	system("iptables -P INPUT DROP");
	system("iptables -P OUTPUT DROP");
	system("iptables -P FORWARD DROP");
	system("iptables -A INPUT -p tcp -s 192.168.1.100 -d 192.168.1.105 --sport 513:65535 --dport 22 -m state --state NEW,ESTABLISHED -j ACCEPT");
	system("iptables -A OUTPUT -p tcp -s 192.168.1.105 -d 192.168.1.100 --sport 22 --dport 513:65535 -m state --state ESTABLISHED -j ACCEPT");
	system("ip addr del 169.254.109.254/16 dev wlan0");
	
	// ### Initialize PHY ######################################################
	phy_init();

	// ### Initialize socket ###################################################
	fd_sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd_sock < 0)
	{
		printf("Socket create error\n");
		return -1;
	}
	saddr_len = sizeof(saddr);

	// ### Initialize thread ###################################################
	// *** Create ***
	// *** Uplink **************************************************************
	if (pthread_create(&thread_recveth, NULL, recveth_handler, NULL) != 0)
		printf("Thread receive ETH create error\n");
	if (pthread_create(&thread_sendirc, NULL, sendirc_handler, NULL) != 0)
		printf("Thread send WLAN create error\n");

	// *** Downlink ************************************************************
	if (pthread_create(&thread_recvvlc, NULL, recvvlc_handler, NULL) != 0)
		printf("Thread receive WLAN create error\n");
	if (pthread_create(&thread_sendeth, NULL, sendeth_handler, NULL) != 0)
		printf("Thread send ETH create error\n");
	
	// *** ACK *****************************************************************
	if (pthread_create(&thread_sendack, NULL, sendack_handler, NULL) != 0)
		printf("Thread send ACK create error\n");	

	// *** Join ***
	// *** Uplink **************************************************************
	pthread_join(thread_recveth, NULL);
	pthread_join(thread_sendirc, NULL);

	// *** Downlink ************************************************************
	pthread_join(thread_recvvlc, NULL);
	pthread_join(thread_sendeth, NULL);

	// *** ACK *****************************************************************
	pthread_join(thread_sendack, NULL);
	
	// ### Test code ###########################################################
	// *** Receive OFDM symbol test ***
	// struct ofdmsym_t ofdmsym = {0};
	// for (int i = 0; i <= 1000; i++)
	// {
		// recv_ofdm_sym(&ofdmsym);
		// for (uint8_t i = 0; i < OFDM_WORD; i++)
			// printf("0x%08X ", ofdmsym.data[i]);
		// printf("\n");
	// }
	
	// *** Send OOK symbol test ***
	// for (int i = 0; i <= 255; i++)
		// send_ook_sym(i);

	// *** Receive OFDM frame test ***
	// for (int k = 0; k <= 200; k++)
	// {
		// struct ethfrm_t ethfrm_d = {0};
		// if (recv_vlc_frm(&ethfrm_d) == HEADER_MISSING)
		// {		
			// printf("VLC header missing\n");
			// continue;
		// }
		// ethfrm_print(ethfrm_d);
	// }

	// *** Send OOK frame test ***
	// for (int k = 0; k <= 200; k++)
	// {
		// struct ethfrm_t ethfrm_u = {0};
		// ethfrm_u.bytes = 1518;
		// for (int i = 0; i < ethfrm_u.bytes; i++)
			// ethfrm_u.data[i] = k;
		
		
		// send_irc_frm(ethfrm_u);
		
		// ethfrm_print(ethfrm_u);
	// }
	
	// *** Receive OFDM ACK test ***
	// for (int k = 0; k <= 10; k++)
	// {
		// struct ethfrm_t ethfrm_d = {0};
		// if (recv_vlc_frm(&ethfrm_d) == ACK_FOUND)
		// {		
			// printf("ACK found\n");
			// continue;
		// }
		// ethfrm_print(ethfrm_d);
	// }

	// *** Send OOK ACK test ***
	// for (int k = 0; k <= 10; k++)
		// send_ack();
	
	return 0;
}

// ### Functions ###############################################################
void phy_init()
{
	// *** Handle to physical memory ***
	if ((fd_mem = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
		perror("Couldn't open the /dev/mem");
	
	// *** Map a page of memory PHY ***
	vlc_rx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd_mem, AXI_VLC_RX);
	ook_tx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd_mem, AXI_IRC_TX);
			
	// PHY initialization
	*(vlc_rx_p+0) = 0x2;
}

void ethfrm_set(ethfrm_t *ethfrm, uint8_t *data, uint16_t bytes)
{
	uint16_t i;

	ethfrm->bytes = bytes;
	for (i = 0; i < ethfrm->bytes; i++)
	{
		ethfrm->data[i] = data[i];
	}
}

void ethfrm_print(struct ethfrm_t ethfrm)
{
	uint16_t i;

	printf("Ethernet frame:\n");
	for (i = 0; i < ethfrm.bytes; i++)
		printf("%02X ", ethfrm.data[i]);
	printf("\n");
}

uint8_t recv_vlc_frm(struct ethfrm_t *ethfrm)
{
	struct ofdmsym_t ofdmsym = {0};
	uint16_t num_ofdm, num_rem_bit, num_rem_byte;
	uint8_t data[FRAM_SIZE] = {0};
	uint16_t data_idx = 0;
	uint16_t i, j;

	// *** Receive OFDM header symbol ***
	recv_ofdm_sym(&ofdmsym);
	// *** Check ID ***
	if (!((ofdmsym.data[0] == 0x16808880) && ((ofdmsym.data[1] & 0xFFFF0000) == 0x16800000)))
		return HEADER_MISSING;
	// *** Check ACK ***
	if ((ofdmsym.data[1] & 0x0000FFFF) == 0x0000FFFF)
		return ACK_FOUND;
	
	num_ofdm = (uint16_t)((ofdmsym.data[2] & 0xFFFF0000) >> 16);
	num_rem_bit = (uint16_t)(ofdmsym.data[2] & 0x0000FFFF);
	// printf("Number of OFDM symbol: %d\n", num_ofdm);
	// printf("Remaining bit: %d\n", num_rem_bit);

	// *** Split all the OFDM symbol into bytes, except the last OFDM symbol ***
	for (i = 0; i < num_ofdm; i++)
	{
		// Receive one OFDM symbol (blocking)
		recv_ofdm_sym(&ofdmsym);
		// *** Split one OFDM symbol to bytes ***
		for (j = 0; j < OFDM_BYTE; j++)
		{
			if (j < 4)
				data[data_idx++] |= (ofdmsym.data[0] >> (24-(j%4)*8));
			else if (j < 8)
				data[data_idx++] |= (ofdmsym.data[1] >> (24-(j%4)*8));
			else if (j < 12)
				data[data_idx++] |= (ofdmsym.data[2] >> (24-(j%4)*8));
			else
				data[data_idx++] |= (ofdmsym.data[3] >> (24-(j%4)*8));
		}
	}

	// *** Split the last OFDM symbol into bytes ***
	if (num_rem_bit)
	{
		// Calculate number of remaining bytes in the last OFDM symbol
		num_rem_byte = num_rem_bit / 8;
		// Receive the last OFDM symbol (blocking)
		recv_ofdm_sym(&ofdmsym);
		// *** Split the last OFDM symbol to bytes ***
		for (i = 0; i < num_rem_byte; i++)
		{
			if (i < 4)
				data[data_idx++] |= (ofdmsym.data[0] >> (24-(i%4)*8));
			else if (i < 8)
				data[data_idx++] |= (ofdmsym.data[1] >> (24-(i%4)*8));
			else if (i < 12)
				data[data_idx++] |= (ofdmsym.data[2] >> (24-(i%4)*8));
			else
				data[data_idx++] |= (ofdmsym.data[3] >> (24-(i%4)*8));
		}
	}

	// *** Construct ethrenet frame ***
	for (i = 0; i < data_idx; i++)
	{
		ethfrm->data[i] = data[i];
	}
	ethfrm->bytes = data_idx;
	// printf("OFDM frame size: %d byte = (%d symbol * %d bit) + %d bit\n", ethfrm->bytes,
			// num_ofdm, OFDM_BIT, num_rem_bit);

	return 0;	// Success receive
}

void send_irc_frm(ethfrm_t ethfrm)
{
	uint16_t i;

	// printf("OOK frame size: %d byte\n", ethfrm.bytes);
	
	// *** Send OOK header ***
	// *** Send ID ***
	send_ook_sym(0x16);
	send_ook_sym(0x80);
	send_ook_sym(0x88);
	send_ook_sym(0x80);
	// *** This is data frame ***
	send_ook_sym(0x00);
	// *** Send number of bytes ***
	send_ook_sym((uint8_t)(ethfrm.bytes >> 8));
	send_ook_sym((uint8_t)(ethfrm.bytes & 0xFF));

	// *** Send OOK data ***
	for (i = 0; i < ethfrm.bytes; i++)
	{
		send_ook_sym(ethfrm.data[i]);
	}
}

void send_ack()
{
	struct ethfrm_t ack = {0};
	uint8_t i;
	
	// *** Construct ACK pattern ***
	ack.data[0] = 0x16;
	ack.data[1] = 0x80;
	ack.data[2] = 0x88;
	ack.data[3] = 0x80;
	ack.data[4] = 0xFF;
	ack.data[5] = 0x00;
	ack.data[6] = 0x00;
	ack.bytes = 7;

	// *** Send IRC ACK ***	
	for (i = 0; i < ack.bytes; i++)
	{
		pthread_mutex_lock(&mutex_ooktx);
		send_ook_sym(ack.data[i]);
		pthread_mutex_unlock(&mutex_ooktx);
	}
	
}

void recv_ofdm_sym(ofdmsym_t *ofdmsym)
{
	// Wait until ready flag is set
	while (!(*(vlc_rx_p+0) & (1 << 2)));

	// *** Read data from data register ***
	ofdmsym->data[0] = *(vlc_rx_p+4);
	ofdmsym->data[1] = *(vlc_rx_p+5);
	ofdmsym->data[2] = *(vlc_rx_p+6);
	ofdmsym->data[3] = *(vlc_rx_p+7);
	// for (uint8_t i = 0; i < OFDM_WORD; i++)
		// printf("0x%08X ", ofdmsym->data[i]);
	// printf("\n");
}

void send_ook_sym(uint8_t data)
{
	// Write data to data register
	*(ook_tx_p+1) = data;
	
	// Wait until busy flag is cleared
	while ((*(ook_tx_p+0) & (1 << 17)));
}

unsigned short checksum(unsigned short* buff, int _16bitword)
{
	unsigned long sum;
	unsigned short ans;

	for(sum = 0; _16bitword > 0; _16bitword--)
		sum += htons(*(buff)++);
	
	sum = ((sum >> 16) + (sum & 0xFFFF));
	sum += (sum >> 16);
	ans = (unsigned short)(~sum);
	ans = ((ans & 0x00FF) << 8) + ((ans & 0xFF00) >> 8);
}

unsigned short checksumtcp(const char *buff, unsigned size)
{
	unsigned sum = 0;
	int i;

	for (i = 0; i < size-1; i += 2)
	{
		unsigned short word16 = *(unsigned short *)&buff[i];
		sum += word16;
	}

	if (size & 1)
	{
		unsigned short word16 = (unsigned char)buff[i];
		sum += word16;
	}

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

void *recveth_handler()
{
	while (1)
	{
		// *** Receive Ethernet frame ***
		ethfrm_t ethfrm_rd = {0};
		ethfrm_rd.bytes = recvfrom(fd_sock, ethfrm_rd.data, FRAM_SIZE, 0,
				&saddr, (socklen_t *)&saddr_len);
		
		// *** Get Ethernet frame information ***
		struct ethhdr *eth = (struct ethhdr *)(ethfrm_rd.data);
		struct iphdr *ip = (struct iphdr *)(ethfrm_rd.data + sizeof(struct ethhdr));

		// *** Check Ethernet frame ***
		if ((ntohs(ip->tot_len)+14) > 1600 || ethfrm_rd.bytes > 1600)
			continue;
		if (!(eth->h_proto == 8 &&
				((unsigned char)eth->h_dest[0] == mac_ethernet[0] && 
				(unsigned char)eth->h_dest[1] == mac_ethernet[1] && 
				(unsigned char)eth->h_dest[2] == mac_ethernet[2] &&
				(unsigned char)eth->h_dest[3] == mac_ethernet[3] &&
				(unsigned char)eth->h_dest[4] == mac_ethernet[4] &&
				(unsigned char)eth->h_dest[5] == mac_ethernet[5])))
			continue;

		// *** Push Ethernet frame to uplink buffer ***
		buff_up_push(ethfrm_rd);
	}
}

void *sendirc_handler()
{
	uint8_t resend_val = 0;
	uint32_t wait = 0;
	
	while (1)
	{
		// *** Read Ethernet frame from uplink buffer ***
		ethfrm_t ethfrm_rd = {0};
		if (buff_up_pop(&ethfrm_rd) == 1)
			continue;
		// ethfrm_print(ethfrm_rd);

		// *** Clear ACK ***
		// pthread_mutex_lock(&mutex_ack);
		// ack = 0;
		// pthread_mutex_unlock(&mutex_ack);

		// *** Send with stop-and-wait ARQ ***
		// resend:
		// Send IRC frame
		send_irc_frm(ethfrm_rd);

		// *** Wait until get ACK ***
		// while (ack == 0)
		// {
			// if (resend_val >= 1)
			// {
				// printf("Resend\n");
				// resend_val = 0;
				// break;
			// }
			// if (wait >= 20000000)
			// {
				// wait = 0;
				// resend_val++;
				// goto resend;
			// }
			// wait++;
		// }
		
		// *** Wait ***
		// struct timespec tim;
		// tim.tv_sec = 0;
		// tim.tv_nsec = 50000UL;
		// nanosleep(&tim,NULL);
	}
}

void *sendack_handler()
{	
	while(1)
	{
		// *** Wait until there is send ACK request ***
		while (send_ack_flag == 0);
		pthread_mutex_lock(&mutex_ackflag);
		send_ack_flag = 0;
		pthread_mutex_unlock(&mutex_ackflag);
		
		// *** Send ACK ***
		send_ack();
	}
}

uint8_t buff_up_push(ethfrm_t ethfrm)
{
	uint8_t stat = 1;	// Push fail (buffer full)

	if (!full_up)
	{
		pthread_mutex_lock(&mutex_up);

		// *** Write eth frame to buffer and update head ***
		buff_up[head_up++] = ethfrm;
		if (head_up == BUFF_UP_SIZE)
			head_up = 0;

		// *** Update empty and full status ***
		empty_up = 0;
		if (head_up == tail_up)
			full_up = 1;

		pthread_mutex_unlock(&mutex_up);

		stat = 0;	// Push success
	}

	return stat;
}

uint8_t buff_up_pop(ethfrm_t *ethfrm)
{
	uint8_t stat = 1;	// Pop fail (buffer empty)

	if (!empty_up)
	{
		pthread_mutex_lock(&mutex_up);

		// *** Read eth frame from buffer and update tail ***
		*ethfrm = buff_up[tail_up++];
		if (tail_up == BUFF_UP_SIZE)
			tail_up = 0;

		// *** Update empty and full status ***
		full_up = 0;
		if (tail_up == head_up)
			empty_up = 1;

		pthread_mutex_unlock(&mutex_up);

		stat = 0;	// Pop success
	}

	return stat;
}

void buff_up_print(void)
{
	uint16_t i, j;

	printf("Buffer Uplink: Head=%d, Tail=%d, Empty=%d, Full=%d\n", head_up,
			tail_up, empty_up, full_up);
	for (i = 0; i < BUFF_UP_SIZE; i++)
	{
		for (j = 0; j < buff_up[i].bytes; j++)
			printf("%02X ", buff_up[i].data[j]);
		printf("\n");
	}
}

void *recvvlc_handler()
{
	uint8_t ret_val;
	
	while (1)
	{
		// *** Reveive data from VLC ***
		ethfrm_t ethfrm_rd = {0};	
		ret_val = recv_vlc_frm(&ethfrm_rd);
		
		// *** If header missing ***
		// while (recv_vlc_frm(&ethfrm_rd) == HEADER_MISSING);
		if (ret_val == HEADER_MISSING)
		{
			printf("VLC header missing\n");
			continue;
		}
		
		// *** If it is ACK ***
		if (ret_val == ACK_FOUND)
		{
			printf("ACK found\n");
			pthread_mutex_lock(&mutex_ack);
			ack = 1;
			pthread_mutex_unlock(&mutex_ack);
			continue;
		}
		
		// *** If it is data, we must send ACK ***
		// pthread_mutex_lock(&mutex_ackflag);
		// send_ack_flag = 1;
		// pthread_mutex_unlock(&mutex_ackflag);
		
		// *** Push Ethernet frame to downlink buffer ***
		buff_dl_push(ethfrm_rd);
		// ethfrm_print(ethfrm_rd);
	}
}

void *sendeth_handler()
{
	uint16_t i;

	// *** Getting index of own interface ***
	struct ifreq ifreq_iface;
	memset(&ifreq_iface, 0, sizeof(ifreq_iface));
	strncpy(ifreq_iface.ifr_name, ETH_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock, SIOCGIFINDEX, &ifreq_iface)) < 0) 
		printf("Error in index ioctl reading %s\n", ETH_IFACE);

	// *** Getting own MAC address ***
	struct ifreq ifreq_mac;
	memset(&ifreq_mac, 0, sizeof(ifreq_mac));
	strncpy(ifreq_mac.ifr_name, ETH_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock, SIOCGIFHWADDR, &ifreq_mac)) < 0)
		printf("Error in SIOCGIFHWADDR ioctl reading %s\n", ETH_IFACE);

	// *** Getting own IP address ***
	struct ifreq ifreq_ip;
	memset(&ifreq_ip, 0, sizeof(ifreq_ip));
	strncpy(ifreq_ip.ifr_name, ETH_IFACE, IFNAMSIZ-1);
	if (ioctl(fd_sock, SIOCGIFADDR, &ifreq_ip) < 0)
		printf("Error in SIOCGIFADDR %s\n", ETH_IFACE);

	while (1)
	{
		// *** Read Ethernet frame from downlink buffer ***
		ethfrm_t ethfrm_rd = {0};
		if (buff_dl_pop(&ethfrm_rd) == 1)
			continue;
		//ethfrm_print(ethfrm_rd);

		// *** Get Ethernet frame information ***
		struct ethhdr *eth = (struct ethhdr*)(ethfrm_rd.data);
		struct iphdr *ip = (struct iphdr*)(ethfrm_rd.data + sizeof(struct ethhdr));
		unsigned short iphdrlen = ip->ihl * 4;

		// *** Check Ethernet frame ***
		if (eth->h_proto != 8)
			continue;

		// *** Send Ethernet frame ***
		if(ip->protocol == 1)	// ICMP packet
		{
			// printf("ICMP downlink packet found\n");

			// Extract ICMP header
			struct icmphdr *icmp = (struct icmphdr*)(ethfrm_rd.data + iphdrlen + sizeof(struct ethhdr));

			// *** Extract Data ***
			ethfrm_rd.bytes = ntohs(ip->tot_len) + 14;
			unsigned char *data = (ethfrm_rd.data + iphdrlen + sizeof(struct ethhdr) + sizeof(struct icmphdr));
			int remaining_data = ethfrm_rd.bytes - (iphdrlen + sizeof(struct ethhdr) + sizeof(struct icmphdr));

			// *** Constructing Ethernet header ***
			ethfrm_t ethfrm_wr = {0};
			struct ethhdr *eth = (struct ethhdr*)(ethfrm_wr.data);
			eth->h_source[0] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[0]);
			eth->h_source[1] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[1]);
			eth->h_source[2] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[2]);
			eth->h_source[3] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[3]);
			eth->h_source[4] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[4]);
			eth->h_source[5] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[5]);
			eth->h_dest[0] = mac_laptop[0];
			eth->h_dest[1] = mac_laptop[1];
			eth->h_dest[2] = mac_laptop[2];
			eth->h_dest[3] = mac_laptop[3];
			eth->h_dest[4] = mac_laptop[4];
			eth->h_dest[5] = mac_laptop[5];
			eth->h_proto = htons(ETH_P_IP);
			ethfrm_wr.bytes += sizeof(struct ethhdr);

			// *** Constructing IP header ***
			struct iphdr *iph = (struct iphdr*)(ethfrm_wr.data + sizeof(struct ethhdr));
			iph->ihl = ip->ihl;
			iph->version = ip->version;
			iph->tos = ip->tos;
			iph->id = ip->id;
			iph->frag_off = ip->frag_off;
			iph->ttl = ip->ttl;
			iph->protocol = ip->protocol;
			iph->saddr = ip->saddr;
			iph->daddr = inet_addr(ip_laptop);
			iph->tot_len = ip->tot_len;
			ethfrm_wr.bytes += sizeof(struct iphdr);
			for(i = 0; i < (iphdrlen-sizeof(struct iphdr)); i++)
			{
				iph[ethfrm_wr.bytes] = ip[ethfrm_wr.bytes];
				ethfrm_wr.bytes++;
			}

			// *** Constructing ICMP header ***
			struct icmphdr *ih = (struct icmphdr*)(ethfrm_wr.data + iphdrlen + sizeof(struct ethhdr));
			ih->type = icmp->type;
			ih->code = icmp->code;
			ih->un.echo.sequence = icmp->un.echo.sequence;
			ih->un.echo.id = icmp->un.echo.id;
			ih->un.frag.mtu = icmp->un.frag.mtu;
			ethfrm_wr.bytes += sizeof(struct icmphdr);

			// *** Constructing payload ***
			for (i = 0; i < remaining_data; i++)
			{
				ethfrm_wr.data[ethfrm_wr.bytes++] = data[i];
			}

			// *** Completing ICMP and IP header ***
			ih->checksum = checksum((unsigned short *)(ethfrm_wr.data + iphdrlen + sizeof(struct ethhdr)),
					(sizeof(struct icmphdr)/2+remaining_data/2));
			iph->tot_len = htons(ethfrm_wr.bytes - sizeof(struct ethhdr));
			iph->check = checksum((unsigned short *)(ethfrm_wr.data + sizeof(struct ethhdr)), iphdrlen/2);

			// *** Actual sending ***
			struct sockaddr_ll sadr_ll;
			sadr_ll.sll_ifindex = ifreq_iface.ifr_ifindex;
			sadr_ll.sll_halen = ETH_ALEN;
			sadr_ll.sll_addr[0] = mac_laptop[0];
			sadr_ll.sll_addr[1] = mac_laptop[1];
			sadr_ll.sll_addr[2] = mac_laptop[2];
			sadr_ll.sll_addr[3] = mac_laptop[3];
			sadr_ll.sll_addr[4] = mac_laptop[4];
			sadr_ll.sll_addr[5] = mac_laptop[5];
			unsigned int res = sendto(fd_sock, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			// if (res < 0)
				// printf("ICMP downlink packet send error\n");
			// else
				// printf("ICMP downlink packet send success\n");
		}
		else if (ip->protocol == 6)		// TCP packet
		{
			// printf("TCP downlink packet found\n");
			
			// *** Extract TCP header ***
			struct tcphdr *tcp = (struct tcphdr*)(ethfrm_rd.data + iphdrlen + sizeof(struct ethhdr));
		
			// *** Extract Data ***
			ethfrm_rd.bytes = ntohs(ip->tot_len) + 14;
			unsigned char *data = (ethfrm_rd.data + iphdrlen + sizeof(struct ethhdr) + sizeof(struct tcphdr));
			int remaining_data = ethfrm_rd.bytes - (iphdrlen + sizeof(struct ethhdr) + sizeof(struct tcphdr));

			// *** Constructing Ethernet header ***
			ethfrm_t ethfrm_wr = {0};
			struct ethhdr *eth = (struct ethhdr*)(ethfrm_wr.data);
			eth->h_source[0] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[0]);
			eth->h_source[1] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[1]);
			eth->h_source[2] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[2]);
			eth->h_source[3] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[3]);
			eth->h_source[4] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[4]);
			eth->h_source[5] = (unsigned char)(ifreq_mac.ifr_hwaddr.sa_data[5]);
			eth->h_dest[0] = mac_laptop[0];
			eth->h_dest[1] = mac_laptop[1];
			eth->h_dest[2] = mac_laptop[2];
			eth->h_dest[3] = mac_laptop[3];
			eth->h_dest[4] = mac_laptop[4];
			eth->h_dest[5] = mac_laptop[5];
			eth->h_proto = htons(ETH_P_IP);
			ethfrm_wr.bytes += sizeof(struct ethhdr);

			// *** Constructing IP header ***
			struct iphdr *iph = (struct iphdr*)(ethfrm_wr.data + sizeof(struct ethhdr));
			iph->ihl = ip->ihl;
			iph->version = ip->version;
			iph->tos = ip->tos;
			iph->id = ip->id;
			iph->frag_off = ip->frag_off;
			iph->ttl = ip->ttl;
			iph->protocol = ip->protocol;
			iph->saddr = ip->saddr;
			iph->daddr = inet_addr(ip_laptop);
			iph->tot_len = ip->tot_len;
			ethfrm_wr.bytes += sizeof(struct iphdr);
			for(i = 0; i < (iphdrlen-sizeof(struct iphdr)); i++)
			{
				iph[ethfrm_wr.bytes] = ip[ethfrm_wr.bytes];
				ethfrm_wr.bytes++;
			}

			// *** Constructing TCP header
			struct tcphdr *th = (struct tcphdr*)(ethfrm_wr.data + iphdrlen + sizeof(struct ethhdr));
			th->th_sport = tcp->th_sport;
			th->th_dport = tcp->th_dport;
			th->th_seq = tcp->th_seq;
			th->th_ack = tcp->th_ack;
			th->th_x2 = tcp->th_x2;
			th->th_off = tcp->th_off;
			th->th_flags = tcp->th_flags;
			th->th_win = tcp->th_win;
			th->th_urp = tcp->th_urp;
			ethfrm_wr.bytes += sizeof(struct tcphdr);
				
			// *** Constructing payload ***
			for(i = 0; i < remaining_data; i++)
			{
				ethfrm_wr.data[ethfrm_wr.bytes++] = data[i];
			}

			// *** Pseudo TCP header ***
			uint8_t *tcpsumbuff = (uint8_t *)malloc(2000);
			int total_len_sum = 0;
			memset(tcpsumbuff,0,2000);
			struct pseudotcp_t *pseudotcp = (struct pseudotcp_t*)(tcpsumbuff);			
			pseudotcp->saddr = iph->saddr;
			pseudotcp->daddr = iph->daddr;
			pseudotcp->zero = 0;
			pseudotcp->protocol = IPPROTO_TCP;
			pseudotcp->len = htons(ntohs(ip->tot_len) - iphdrlen);
			total_len_sum += sizeof(struct pseudotcp_t);
			struct tcphdr *thsum = (struct tcphdr *)(tcpsumbuff + sizeof(struct pseudotcp_t));
			thsum->th_sport = tcp->th_sport;
			thsum->th_dport = tcp->th_dport;
			thsum->th_seq = tcp->th_seq;
			thsum->th_ack = tcp->th_ack;
			thsum->th_x2 = tcp->th_x2;
			thsum->th_off = tcp->th_off;
			thsum->th_flags = tcp->th_flags;
			thsum->th_win = tcp->th_win;
			thsum->th_urp = tcp->th_urp;
			thsum->th_sum = 0;
			total_len_sum += sizeof(struct tcphdr);
			for(i = 0; i < remaining_data; i++)
			{
				tcpsumbuff[total_len_sum++] = data[i];
			}
				
			// *** Completing TCP and IP header ***
			th->th_sum = checksumtcp((char*)(tcpsumbuff), sizeof(struct pseudotcp_t) + ntohs(ip->tot_len) - iphdrlen);
			iph->check = checksum((unsigned short*)(ethfrm_wr.data + sizeof(struct ethhdr)), iphdrlen/2);

			//Actual sending
			struct sockaddr_ll sadr_ll;
			sadr_ll.sll_ifindex = ifreq_iface.ifr_ifindex;
			sadr_ll.sll_halen = ETH_ALEN;
			sadr_ll.sll_addr[0] = mac_laptop[0];
			sadr_ll.sll_addr[1] = mac_laptop[1];
			sadr_ll.sll_addr[2] = mac_laptop[2];
			sadr_ll.sll_addr[3] = mac_laptop[3];
			sadr_ll.sll_addr[4] = mac_laptop[4];
			sadr_ll.sll_addr[5] = mac_laptop[5];

			unsigned int res = sendto(fd_sock, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			// if (res < 0)
				// printf("TCP downlink packet send error\n");
			// else
				// printf("TCP downlink packet send success\n");
		}
	}
}

uint8_t buff_dl_push(ethfrm_t ethfrm)
{
	uint8_t stat = 1;	// Push fail (buffer full)

	if (!full_dl)
	{
		pthread_mutex_lock(&mutex_dl);

		// *** Write eth frame to buffer and update head ***
		buff_dl[head_dl++] = ethfrm;
		if (head_dl == BUFF_DL_SIZE)
			head_dl = 0;

		// *** Update empty and full status ***
		empty_dl = 0;
		if (head_dl == tail_dl)
			full_dl = 1;

		pthread_mutex_unlock(&mutex_dl);

		stat = 0;	// Push success
	}

	return stat;
}

uint8_t buff_dl_pop(ethfrm_t *ethfrm)
{
	uint8_t stat = 1;	// Pop fail (buffer empty)

	if (!empty_dl)
	{
		pthread_mutex_lock(&mutex_dl);

		// *** Read eth frame from buffer and update tail ***
		*ethfrm = buff_dl[tail_dl++];
		if (tail_dl == BUFF_DL_SIZE)
			tail_dl = 0;

		// *** Update empty and full status ***
		full_dl = 0;
		if (tail_dl == head_dl)
			empty_dl = 1;

		pthread_mutex_unlock(&mutex_dl);

		stat = 0;	// Pop success
	}

	return stat;
}

void buff_dl_print(void)
{
	uint16_t i, j;

	printf("Buffer Downlink: Head=%d, Tail=%d, Empty=%d, Full=%d\n", head_dl,
			tail_dl, empty_dl, full_dl);
	for (i = 0; i < BUFF_DL_SIZE; i++)
	{
		for (j = 0; j < buff_dl[i].bytes; j++)
			printf("%02X ", buff_dl[i].data[j]);
		printf("\n");
	}
}
