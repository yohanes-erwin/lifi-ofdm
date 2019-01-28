// *** Author: Erwin Ouyang
// *** Date  : 28 Sep 2018
// *** Note  : Adapted from Fuad Ismail's code

// ### Description #############################################################
// This code is for LiFi access point
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
// This iface is connected to WiFi router
#define WLAN_IFACE				"wlan0"
// WiFi router, TP-Link_MR3020, 192.168.1.1
#define MAC_WIFI_ROUTER_1		0x7C
#define MAC_WIFI_ROUTER_2		0x8B 
#define MAC_WIFI_ROUTER_3		0xCA
#define MAC_WIFI_ROUTER_4		0x42
#define MAC_WIFI_ROUTER_5		0x9F
#define MAC_WIFI_ROUTER_6		0xE2
// WLAN iface, wlan0, 192.168.1.105
#define MAC_WLAN_1				0x74
#define MAC_WLAN_2				0xDA
#define MAC_WLAN_3				0x38
#define MAC_WLAN_4				0xA8
#define MAC_WLAN_5				0x87
#define MAC_WLAN_6				0x10

// ### Defines #################################################################
// *** PHY address ***
#define AXI_VLC_TX		0x41200000
#define AXI_IRC_RX 		0x41230000
// *** Ethernet ***
#define FRAM_SIZE		1518
// *** OFDM ***
#define OFDM_WORD		4
#define OFDM_BYTE		15
#define OFDM_BIT		120

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
static volatile uint32_t *vlc_tx_p;
static volatile uint32_t *ook_rx_p;
// *** Socket ***
int fd_sock;
struct sockaddr saddr;
int saddr_len;

// *** Uplink ******************************************************************
// *** Thread ***
pthread_t thread_recvirc, thread_sendwlan;
pthread_mutex_t mutex_up = PTHREAD_MUTEX_INITIALIZER;
// *** Ethernet frame fifo buffer ***
struct ethfrm_t buff_up[BUFF_UP_SIZE];
uint16_t head_up = 0, tail_up = 0;
uint8_t empty_up = 1, full_up = 0;
// WiFi router's MAC
uint8_t mac_wifi_router[6] = 
{
	MAC_WIFI_ROUTER_1,
	MAC_WIFI_ROUTER_2,
	MAC_WIFI_ROUTER_3,
	MAC_WIFI_ROUTER_4,
	MAC_WIFI_ROUTER_5,
	MAC_WIFI_ROUTER_6
};

// *** Downlink ****************************************************************
// *** Thread ***
pthread_t thread_recvwlan, thread_sendvlc;
pthread_mutex_t mutex_dl = PTHREAD_MUTEX_INITIALIZER;
// *** Ethernet frame fifo buffer ***
struct ethfrm_t buff_dl[BUFF_DL_SIZE];
uint16_t head_dl = 0, tail_dl = 0;
uint8_t empty_dl = 1, full_dl = 0;
// WLAN's MAC
uint8_t mac_wlan[6] =
{
	MAC_WLAN_1,
	MAC_WLAN_2,
	MAC_WLAN_3,
	MAC_WLAN_4,
	MAC_WLAN_5,
	MAC_WLAN_6
};

// *** ACK *********************************************************************
uint8_t ack = 0;
uint8_t send_ack_flag = 0;
pthread_t thread_sendack;
pthread_mutex_t mutex_ack, mutex_ofdmtx, mutex_ackflag = 
		PTHREAD_MUTEX_INITIALIZER;

// ### Function prototypes #####################################################
// *** PHY layer initialization ***
void phy_init(void);

// *** Ethernet frame functions *** 
void ethfrm_set(ethfrm_t *ethfrm, uint8_t *data, uint16_t bytes);
void ethfrm_print(ethfrm_t ethfrm);
// *** Data link layer functions ***
void send_vlc_frm(struct ethfrm_t ethfrm);
uint8_t recv_irc_frm(struct ethfrm_t *ethfrm);
void send_ack(void);
// *** PHY layer functions ***
void send_ofdm_sym(ofdmsym_t ofdmsym);
void recv_ook_sym(uint8_t *data);

// *** Checksum ***
unsigned short checksum(unsigned short *buff, int _16bitword);
unsigned short checksumtcp(const char *buff, unsigned size);

// *** Uplink ******************************************************************
// *** Thread handler ***
void *recvirc_handler();
void *sendwlan_handler();
// *** FIFO buffer functions ***
uint8_t buff_up_push(ethfrm_t ethfrm);
uint8_t buff_up_pop(ethfrm_t *ethfrm);
void buff_up_print(void);

// *** Downlink ****************************************************************
// *** Thread handler ***
void *recvwlan_handler();
void *sendvlc_handler();
// *** FIFO buffer functions ***
uint8_t buff_dl_push(ethfrm_t ethfrm);
uint8_t buff_dl_pop(ethfrm_t *ethfrm);
void buff_dl_print(void);

// *** ACK *********************************************************************
void *sendack_handler();

// ### Main ####################################################################
int main()
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
	if (pthread_create(&thread_recvirc, NULL, recvirc_handler, NULL) != 0)
		printf("Thread receive ETH create error\n");
	if (pthread_create(&thread_sendwlan, NULL, sendwlan_handler, NULL) != 0)
		printf("Thread send WLAN create error\n");

	// *** Downlink ************************************************************
	if (pthread_create(&thread_recvwlan, NULL, recvwlan_handler, NULL) != 0)
		printf("Thread receive WLAN create error\n");
	if (pthread_create(&thread_sendvlc, NULL, sendvlc_handler, NULL) != 0)
		printf("Thread send ETH create error\n");

	// *** ACK *****************************************************************
	if (pthread_create(&thread_sendack, NULL, sendack_handler, NULL) != 0)
		printf("Thread send ACK create error\n");	
	
	// *** Join ***
	// *** Uplink **************************************************************
	pthread_join(thread_recvirc, NULL);
	pthread_join(thread_sendwlan, NULL);

	// *** Downlink ************************************************************
	pthread_join(thread_recvwlan, NULL);
	pthread_join(thread_sendvlc, NULL);

	// *** ACK *****************************************************************
	pthread_join(thread_sendack, NULL);
	
	// ### Test code ###########################################################
	// *** Send OFDM symbol test ***
	// struct ofdmsym_t ofdmsym = {0};
	// ofdmsym.bytes = 4;
	// for (int i = 0; i <= 1000; i++)
	// {
		// ofdmsym.data[0] = 0x10000000 + (i << 4);
		// ofdmsym.data[1] = 0x20000000 + (i << 4);
		// ofdmsym.data[2] = 0x30000000 + (i << 4);
		// ofdmsym.data[3] = 0x40000000 + (i << 4);
		// send_ofdm_sym(ofdmsym);
	// }
	
	// *** Receive OOK symbol test ***
	// uint8_t data;
	// for (int i = 0; i <= 255; i++)
	// {
		// recv_ook_sym(&data);
		// printf("0x%02X ", data);
	// }
	// printf("\n");

	// *** Send OFDM frame test ***
	// for (int k = 0; k <= 200; k++)
	// {
		// struct ethfrm_t ethfrm_d = {0};
		// ethfrm_d.bytes = 500;
		// for (int i = 0; i < ethfrm_d.bytes; i++)
			 // ethfrm_d.data[i] = k;
		// send_vlc_frm(ethfrm_d);
		// ethfrm_print(ethfrm_d);
	// }
	
	// *** Receive OOK frame test ***
	// for (int k = 0; k <= 200; k++)
	// {
		// struct ethfrm_t ethfrm_u = {0};
		// recv_irc_frm(&ethfrm_u);
		// ethfrm_print(ethfrm_u);
	// }
	
	// *** Send OFDM ACK test ***
	// for (int k = 0; k <= 10; k++)
		// send_ack();

	// *** Receive OOK ACK test ***
	// for (int k = 0; k <= 10; k++)
	// {
		// struct ethfrm_t ethfrm_u = {0};
		// if (recv_irc_frm(&ethfrm_u) == ACK_FOUND)
		// {		
			// printf("ACK found\n");
			// continue;
		// }
		// ethfrm_print(ethfrm_u);
	// }
	
	return 0;
}

// ### Functions ###############################################################
void phy_init()
{
	// *** Handle to physical memory ***
	if ((fd_mem = open("/dev/mem", O_RDWR | O_SYNC)) < 0)
		perror("Couldn't open the /dev/mem");
	
	// *** Map a page of memory PHY ***
	vlc_tx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd_mem, AXI_VLC_TX);
	ook_rx_p = (uint32_t *)mmap(0, getpagesize(), PROT_READ | PROT_WRITE,
			MAP_SHARED, fd_mem, AXI_IRC_RX);
			
	// PHY initialization
	*(vlc_tx_p+0) = 0x122;
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

void send_vlc_frm(struct ethfrm_t ethfrm)
{
	struct ofdmsym_t ofdmsym = {0};
	uint16_t num_ofdm, num_rem_bit;
	uint16_t ethfrm_idx = 0;
	uint16_t i, j;

	// *** Split an Ethernet frame into OFDM symbols ***
	// Calculate how many OFDM symbols that we can make
	num_ofdm = ethfrm.bytes * 8 / OFDM_BIT;
	// Calculate how many remaining bits for the last OFDM symbol
	num_rem_bit = ethfrm.bytes * 8 % OFDM_BIT;
	// printf("OFDM frame size: %d byte = (%d symbol * %d bit) + %d bit\n", ethfrm.bytes,
			// num_ofdm, OFDM_BIT, num_rem_bit);

	// *** Send the first OFDM symbol (header symbol) ***
	// *** Fill OFDM symbol ***
	ofdmsym.data[0] = 0x16808880;
	ofdmsym.data[1] = 0x16800000;
	ofdmsym.data[2] = (num_ofdm << 16) | num_rem_bit;
	ofdmsym.data[3] = 0x00000000; 
	ofdmsym.bytes = OFDM_BYTE;
	// *** Send OFDM symbol ***
	send_ofdm_sym(ofdmsym);
	
	// *** Send all the OFDM symbol, except the last OFDM symbol ***
	for (i = 0; i < num_ofdm; i++)
	{
		// *** Clear OFDM symbol ***
		for (j = 0; j < OFDM_WORD; j++)
			ofdmsym.data[j] = 0;
		// *** Fill OFDM symbol ***
		for (j = 0; j < OFDM_BYTE; j++)
		{
			if (j < 4)
				ofdmsym.data[0] |= (ethfrm.data[ethfrm_idx++] << (24-(j%4)*8));
			else if (j < 8)
				ofdmsym.data[1] |= (ethfrm.data[ethfrm_idx++] << (24-(j%4)*8));
			else if (j < 12)
				ofdmsym.data[2] |= (ethfrm.data[ethfrm_idx++] << (24-(j%4)*8));
			else
				ofdmsym.data[3] |= (ethfrm.data[ethfrm_idx++] << (24-(j%4)*8));
		}
		ofdmsym.bytes = OFDM_BYTE;
		// *** Send OFDM symbol ***
		send_ofdm_sym(ofdmsym);
	}

	// *** Send the last OFDM symbol ***
	if (num_rem_bit)
	{
		// *** Clear OFDM symbol ***
		for (j = 0; j < OFDM_WORD; j++)
			ofdmsym.data[j] = 0;
		// *** Fill OFDM symbol ***
		for (i = 0; i < OFDM_BYTE; i++)
		{
			uint8_t data = ethfrm.data[ethfrm_idx++];
			if (ethfrm_idx == (ethfrm.bytes+1))
				break;
			if (i < 4)
				ofdmsym.data[0] |= (data << (24-(i%4)*8));
			else if (i < 8)
				ofdmsym.data[1] |= (data << (24-(i%4)*8));
			else if (i < 12)
				ofdmsym.data[2] |= (data << (24-(i%4)*8));
			else
				ofdmsym.data[3] |= (data << (24-(i%4)*8));
		}
		ofdmsym.bytes = num_rem_bit;
		// *** Send OFDM symbol ***
		send_ofdm_sym(ofdmsym);
	}
}

uint8_t recv_irc_frm(struct ethfrm_t *ethfrm)
{
	uint16_t i;

	// *** Receive OOK header ***
	uint8_t header[7];
	for (i = 0; i <= 6; i++)
		recv_ook_sym(&header[i]);
	
	// *** Check ID ***
	if (!((header[0] == 0x16) && (header[1] == 0x80) &&
			(header[2] == 0x88) && (header[3] == 0x80)))
		return HEADER_MISSING;
	// *** Check ACK ***
	if (header[4] == 0xFF)
		return ACK_FOUND;

	// *** Get number of bytes ***
	ethfrm->bytes = (uint16_t)((header[5] << 8) | header[6]);

	// *** Receive OOK data ***
	uint8_t data;
	for (i = 0; i < ethfrm->bytes; i++)
	{
		recv_ook_sym(&data);
		ethfrm->data[i] = data;
	}
	// printf("OOK frame size: %d byte\n", ethfrm->bytes);
	
	return 0;	// Success receive
}

void send_ack()
{
	struct ofdmsym_t ack = {0};
		
	// *** Construct ACK pattern ***
	ack.data[0] = 0x16808880;
	ack.data[1] = 0x1680FFFF;
	ack.data[2] = 0x00000000;
	ack.data[3] = 0x00000000; 
	ack.bytes = OFDM_BYTE;
	
	// *** Send VLC ACK ***
	pthread_mutex_lock(&mutex_ofdmtx);
	send_ofdm_sym(ack);
	pthread_mutex_unlock(&mutex_ofdmtx);
}

void send_ofdm_sym(ofdmsym_t ofdmsym)
{
	// *** Write data to data register ***
	*(vlc_tx_p+4) = ofdmsym.data[0];
	*(vlc_tx_p+5) = ofdmsym.data[1];
	*(vlc_tx_p+6) = ofdmsym.data[2];
	*(vlc_tx_p+7) = ofdmsym.data[3];
	// for (uint8_t i = 0; i < OFDM_WORD; i++)
		// printf("0x%08X ", ofdmsym.data[i]);
	// printf("\n");
	
	// Wait until busy flag is cleared
	while ((*(vlc_tx_p+0) & (1 << 10)));
}

void recv_ook_sym(uint8_t *data)
{
	// Wait until ready flag is set
	while (!(*(ook_rx_p+0) & (1 << 16)));
	
	// *** Copy from PHY RX memory ***
	*data = *(ook_rx_p+1);
	// printf("0x%02X\n", *data);
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

void *recvirc_handler()
{
	uint8_t ret_val;
	
	while (1)
	{
		// *** Receive data from IRC ***
		ethfrm_t ethfrm_rd = {0};
		ret_val = recv_irc_frm(&ethfrm_rd);
		
		// *** If header missing ***
		// while (recv_irc_frm(&ethfrm_rd) == HEADER_MISSING);
		if (ret_val == HEADER_MISSING)
		{
			printf("IRC header missing\n");
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
		
		// *** Push Ethernet frame to uplink buffer ***
		buff_up_push(ethfrm_rd);
		// ethfrm_print(ethfrm_rd);
	}
}

void *sendwlan_handler()
{
	uint16_t i;

	// *** Getting index of own interface ***
	struct ifreq ifreq_iface;
	memset(&ifreq_iface, 0, sizeof(ifreq_iface));
	strncpy(ifreq_iface.ifr_name, WLAN_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock, SIOCGIFINDEX, &ifreq_iface)) < 0) 
		printf("Error in index ioctl reading %s\n", WLAN_IFACE);

	// *** Getting own MAC address ***
	struct ifreq ifreq_mac;
	memset(&ifreq_mac, 0, sizeof(ifreq_mac));
	strncpy(ifreq_mac.ifr_name, WLAN_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock, SIOCGIFHWADDR, &ifreq_mac)) < 0)
		printf("Error in SIOCGIFHWADDR ioctl reading %s\n", WLAN_IFACE);

	// *** Getting own IP address ***
	struct ifreq ifreq_ip;
	memset(&ifreq_ip, 0, sizeof(ifreq_ip));
	strncpy(ifreq_ip.ifr_name, WLAN_IFACE, IFNAMSIZ-1);
	if (ioctl(fd_sock, SIOCGIFADDR, &ifreq_ip) < 0)
		printf("Error in SIOCGIFADDR %s\n", WLAN_IFACE);

	while (1)
	{
		// *** Pop Ethernet frame from uplink buffer ***
		ethfrm_t ethfrm_rd = {0};
		if (buff_up_pop(&ethfrm_rd) == 1)
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
			// printf("ICMP uplink packet found\n");

			// *** Extract ICMP header ***
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
			eth->h_dest[0] = mac_wifi_router[0];
			eth->h_dest[1] = mac_wifi_router[1];
			eth->h_dest[2] = mac_wifi_router[2];
			eth->h_dest[3] = mac_wifi_router[3];
			eth->h_dest[4] = mac_wifi_router[4];
			eth->h_dest[5] = mac_wifi_router[5];
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
			iph->saddr = inet_addr(inet_ntoa((((struct sockaddr_in*) & (ifreq_ip.ifr_addr))->sin_addr)));
			iph->daddr = ip->daddr;
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
			sadr_ll.sll_addr[0] = mac_wifi_router[0];
			sadr_ll.sll_addr[1] = mac_wifi_router[1];
			sadr_ll.sll_addr[2] = mac_wifi_router[2];
			sadr_ll.sll_addr[3] = mac_wifi_router[3];
			sadr_ll.sll_addr[4] = mac_wifi_router[4];
			sadr_ll.sll_addr[5] = mac_wifi_router[5];
			unsigned int res = sendto(fd_sock, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			// if (res < 0)
				// printf("ICMP uplink packet send error\n");
			// else
				// printf("ICMP uplink packet send success\n");
		}
		else if (ip->protocol == 6)		// TCP packet
		{
			// printf("TCP uplink packet found\n");
			
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
			eth->h_dest[0] = mac_wifi_router[0];
			eth->h_dest[1] = mac_wifi_router[1];
			eth->h_dest[2] = mac_wifi_router[2];
			eth->h_dest[3] = mac_wifi_router[3];
			eth->h_dest[4] = mac_wifi_router[4];
			eth->h_dest[5] = mac_wifi_router[5];
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
			iph->saddr = inet_addr(inet_ntoa((((struct sockaddr_in*) & (ifreq_ip.ifr_addr))->sin_addr)));
			iph->daddr = ip->daddr;
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

			// *** Actual sending ***
			struct sockaddr_ll sadr_ll;
			sadr_ll.sll_ifindex = ifreq_iface.ifr_ifindex;
			sadr_ll.sll_halen = ETH_ALEN;
			sadr_ll.sll_addr[0] = mac_wifi_router[0];
			sadr_ll.sll_addr[1] = mac_wifi_router[1];
			sadr_ll.sll_addr[2] = mac_wifi_router[2];
			sadr_ll.sll_addr[3] = mac_wifi_router[3];
			sadr_ll.sll_addr[4] = mac_wifi_router[4];
			sadr_ll.sll_addr[5] = mac_wifi_router[5];

			unsigned int res = sendto(fd_sock, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			// if (res < 0)
				// printf("TCP uplink packet send error\n");
			// else
				// printf("TCP uplink packet send success\n");
		}
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

void *recvwlan_handler()
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
				((unsigned char)eth->h_dest[0] == mac_wlan[0] && 
				(unsigned char)eth->h_dest[1] == mac_wlan[1] && 
				(unsigned char)eth->h_dest[2] == mac_wlan[2] &&
				(unsigned char)eth->h_dest[3] == mac_wlan[3] &&
				(unsigned char)eth->h_dest[4] == mac_wlan[4] &&
				(unsigned char)eth->h_dest[5] == mac_wlan[5])))
			continue;

		// *** Push Ethernet frame to downlink buffer ***
		buff_dl_push(ethfrm_rd);
	}
}

void *sendvlc_handler()
{
	uint8_t resend_val = 0;
	uint32_t wait = 0;
	
	while (1)
	{
		// *** Read Ethernet frame from downlink buffer ***
		ethfrm_t ethfrm_rd = {0};
		if (buff_dl_pop(&ethfrm_rd) == 1)
			continue;
		// ethfrm_print(ethfrm_rd);
	
		// *** Clear ACK ***
		// pthread_mutex_lock(&mutex_ack);
		// ack = 0;
		// pthread_mutex_unlock(&mutex_ack);
		
		// *** Send with stop-and-wait ARQ ***
		// resend:
		// pthread_mutex_lock(&mutex_ofdmtx);
		// Send VLC frame
		send_vlc_frm(ethfrm_rd);
		// pthread_mutex_unlock(&mutex_ofdmtx);

		// *** Wait until get ACK ***
		// while (ack == 0)
		// {
			// if (resend_val >= 1)
			// {
				// resend_val = 0;
				// break;
			// }
			// if (wait >= 200000)
			// {
				// wait = 0;
				// resend_val++;
				// goto resend;
			// }
			// wait++;
		// }
		
		// *** Wait ***
		struct timespec tim;
		tim.tv_sec = 0;
		tim.tv_nsec = 5000UL;
		nanosleep(&tim,NULL);
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
