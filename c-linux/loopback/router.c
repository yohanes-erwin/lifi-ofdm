// Author: Erwin Ouyang
// Date  : 25 Sep 2018
// Update: 25 Sep 2018 - Adapted from Fuad Ismail's code
//         26 Sep 2018 - Porting to RedPitaya
//         28 Jan 2019 - GitHub first commit, from router.c 

// ### Description #############################################################
// Test routing functionality:
// 	pc_ethernet -> red_pi(eth0->wlan0) -> wifi_router -> internet
//	pc_ethernet <- red_pi(eth0<-wlan0) <- wifi_router <- internet

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

// ### Defines #################################################################
// Ethernet frame
#define FRAM_SIZE		1518

// *** Uplink ******************************************************************
// Ring buffer size
#define BUFF_UP_SIZE 	256
// This iface is connected to WiFi router
#define WLAN_IFACE		"wlan0" 

// *** Downlink ****************************************************************
// Ring buffer size
#define BUFF_DL_SIZE 	256
// This iface is connected to laptop
#define ETH_IFACE		"eth0" 

// ### Struct definitions ######################################################
typedef struct ethfrm_t
{
	uint8_t data[FRAM_SIZE];	// Ethernet packet data
	uint16_t bytes;				// Ethernet packet length
} ethfrm_t;

typedef struct pseudo_tcp_t
{
	uint32_t saddr;
	uint32_t daddr;
	uint8_t zero;
	uint8_t protocol;
	uint16_t len;
} pseudo_tcp_t;

// ### Variables ###############################################################
// *** Uplink ******************************************************************
// *** Thread ***
pthread_t thread_recveth, thread_sendwlan;
pthread_mutex_t mutex_up = PTHREAD_MUTEX_INITIALIZER;
// *** Ethernet frame fifo buffer ***
struct ethfrm_t buff_up[BUFF_UP_SIZE];
uint16_t head_up = 0, tail_up = 0;
uint8_t empty_up = 1, full_up = 0;
// *** Socket ***
int fd_sock_up;
struct sockaddr saddr_up;
int saddr_len_up;
// ETH0 interface, 192.168.3.105
uint8_t mac_ethernet[6] = {0x00, 0x26, 0x32, 0xF0, 0x56, 0x70};
// WiFi router, 192.168.1.1
uint8_t mac_wifi_router[6] = {0x7C, 0x8B, 0xCA, 0x42, 0x9F, 0xE2};

// *** Downlink ****************************************************************
// *** Thread ***
pthread_t thread_recvwlan, thread_sendeth;
pthread_mutex_t mutex_dl = PTHREAD_MUTEX_INITIALIZER;
// *** Ethernet frame fifo buffer ***
struct ethfrm_t buff_dl[BUFF_DL_SIZE];
uint16_t head_dl = 0, tail_dl = 0;
uint8_t empty_dl = 1, full_dl = 0;
// *** Socket ***
int fd_sock_dl;
struct sockaddr saddr_dl;
int saddr_len_dl;
// wlan0 interface, 192.168.1.105
uint8_t mac_wlan[6] = {0x74, 0xDA, 0x38, 0xA8, 0x87, 0x10};
// PC Ethernet, 192.168.3.1
uint8_t mac_laptop[6] = {0x00, 0x30, 0x67, 0x0B, 0xE0, 0xFD};
char *ip_laptop = "192.168.3.1";

// ### Function prototypes #####################################################
// *** Ethernet frame functions *** 
void ethfrm_set(ethfrm_t *ethfrm, uint8_t *data, uint16_t bytes);
void ethfrm_print(ethfrm_t ethfrm);

// *** Uplink ******************************************************************
// *** Thread handler ***
void *recveth_handler();
void *sendwlan_handler();
// *** FIFO buffer functions ***
uint8_t buff_up_push(ethfrm_t ethfrm);
uint8_t buff_up_pop(ethfrm_t *ethfrm);
void buff_up_print(void);

// *** Downlink ****************************************************************
// *** Thread handler ***
void *recvwlan_handler();
void *sendeth_handler();
// *** FIFO buffer functions ***
uint8_t buff_dl_push(ethfrm_t ethfrm);
uint8_t buff_dl_pop(ethfrm_t *ethfrm);
void buff_dl_print(void);

// *** Checksum ***
unsigned short checksum(unsigned short *buff, int _16bitword);
unsigned short checksumtcp(const char *buff, unsigned size);

// ### Main ####################################################################
int main(int argc, char *argv[])
{
	//setpriority(PRIO_PROCESS, 0, -20);

	// ### Disable kernel packet processing ####################################
	system("iptables -P INPUT DROP");
	system("iptables -P OUTPUT DROP");
	system("iptables -P FORWARD DROP");
	system("iptables -A INPUT -p tcp -s 192.168.1.100 -d 192.168.1.105 --sport 513:65535 --dport 22 -m state --state NEW,ESTABLISHED -j ACCEPT");
	system("iptables -A OUTPUT -p tcp -s 192.168.1.105 -d 192.168.1.100 --sport 22 --dport 513:65535 -m state --state ESTABLISHED -j ACCEPT");
	system("ip addr del 169.254.109.254/16 dev wlan0");

	// ### Initialize socket ###################################################
	// *** Uplink **************************************************************
	fd_sock_up = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd_sock_up < 0)
	{
		printf("Socket create error\n");
		return -1;
	}
	saddr_len_up = sizeof(saddr_up);

	// *** Downlink ************************************************************
	fd_sock_dl = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd_sock_dl < 0)
	{
		printf("Socket create error\n");
		return -1;
	}
	saddr_len_dl = sizeof(saddr_dl);

	// ### Initialize thread ###################################################
	// *** Create ***
	// *** Uplink **************************************************************
	if (pthread_create(&thread_recveth, NULL, recveth_handler, NULL) != 0)
		printf("Thread receive ETH create error\n");
	if (pthread_create(&thread_sendwlan, NULL, sendwlan_handler, NULL) != 0)
		printf("Thread send WLAN create error\n");

	// *** Downlink ************************************************************
	if (pthread_create(&thread_recvwlan, NULL, recvwlan_handler, NULL) != 0)
		printf("Thread receive WLAN create error\n");
	if (pthread_create(&thread_sendeth, NULL, sendeth_handler, NULL) != 0)
		printf("Thread send ETH create error\n");

	// *** Join ***
	// *** Uplink **************************************************************
	pthread_join(thread_recveth, NULL);
	pthread_join(thread_sendwlan, NULL);

	// *** Downlink ************************************************************
	pthread_join(thread_recvwlan, NULL);
	pthread_join(thread_sendeth, NULL);

	return 0;
}

// ### Thread handler ##########################################################
void *recveth_handler()
{
	while (1)
	{
		// *** Receive Ethernet frame ***
		ethfrm_t ethfrm_rd = {0};
		ethfrm_rd.bytes = recvfrom(fd_sock_up, ethfrm_rd.data, FRAM_SIZE, 0,
				&saddr_up, (socklen_t *)&saddr_len_up);
		
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

void *sendwlan_handler()
{
	uint16_t i;

	// *** Getting index of own interface ***
	struct ifreq ifreq_iface;
	memset(&ifreq_iface, 0, sizeof(ifreq_iface));
	strncpy(ifreq_iface.ifr_name, WLAN_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock_dl, SIOCGIFINDEX, &ifreq_iface)) < 0) 
		printf("Error in index ioctl reading %s\n", WLAN_IFACE);

	// *** Getting own MAC address ***
	struct ifreq ifreq_mac;
	memset(&ifreq_mac, 0, sizeof(ifreq_mac));
	strncpy(ifreq_mac.ifr_name, WLAN_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock_dl, SIOCGIFHWADDR, &ifreq_mac)) < 0)
		printf("Error in SIOCGIFHWADDR ioctl reading %s\n", WLAN_IFACE);

	// *** Getting own IP address ***
	struct ifreq ifreq_ip;
	memset(&ifreq_ip, 0, sizeof(ifreq_ip));
	strncpy(ifreq_ip.ifr_name, WLAN_IFACE, IFNAMSIZ-1);
	if (ioctl(fd_sock_dl, SIOCGIFADDR, &ifreq_ip) < 0)
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
			//printf("ICMP uplink packet found\n");

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
			unsigned int res = sendto(fd_sock_dl, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			//if (res < 0)
			//	printf("ICMP uplink packet send error\n");
			//else
			//	printf("ICMP uplink packet send success\n");
		}
		else if (ip->protocol == 6)		// TCP packet
		{
			//printf("TCP uplink packet found\n");
			
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
			struct pseudo_tcp_t *pseudo_tcp = (struct pseudo_tcp_t*)(tcpsumbuff);			
			pseudo_tcp->saddr = iph->saddr;
			pseudo_tcp->daddr = iph->daddr;
			pseudo_tcp->zero = 0;
			pseudo_tcp->protocol = IPPROTO_TCP;
			pseudo_tcp->len = htons(ntohs(ip->tot_len) - iphdrlen);
			total_len_sum += sizeof(struct pseudo_tcp_t);
			struct tcphdr *thsum = (struct tcphdr *)(tcpsumbuff + sizeof(struct pseudo_tcp_t));
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
			th->th_sum = checksumtcp((char*)(tcpsumbuff), sizeof(struct pseudo_tcp_t) + ntohs(ip->tot_len) - iphdrlen);
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

			unsigned int res = sendto(fd_sock_dl, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			//if (res < 0)
			//	printf("TCP uplink packet send error\n");
			//else
			//	printf("TCP uplink packet send success\n");
		}
	}
}

void *recvwlan_handler()
{
	while (1)
	{
		// *** Receive Ethernet frame ***
		ethfrm_t ethfrm_rd = {0};
		ethfrm_rd.bytes = recvfrom(fd_sock_dl, ethfrm_rd.data, FRAM_SIZE, 0,
				&saddr_dl, (socklen_t *)&saddr_len_dl);
		
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

void *sendeth_handler()
{
	uint16_t i;
	
	// *** Getting index of own interface ***
	struct ifreq ifreq_iface;
	memset(&ifreq_iface, 0, sizeof(ifreq_iface));
	strncpy(ifreq_iface.ifr_name, ETH_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock_up, SIOCGIFINDEX, &ifreq_iface)) < 0) 
		printf("Error in index ioctl reading %s\n", ETH_IFACE);

	// *** Getting own MAC address ***
	struct ifreq ifreq_mac;
	memset(&ifreq_mac, 0, sizeof(ifreq_mac));
	strncpy(ifreq_mac.ifr_name, ETH_IFACE, IFNAMSIZ-1);
	if ((ioctl(fd_sock_up, SIOCGIFHWADDR, &ifreq_mac)) < 0)
		printf("Error in SIOCGIFHWADDR ioctl reading %s\n", ETH_IFACE);

	// *** Getting own IP address ***
	struct ifreq ifreq_ip;
	memset(&ifreq_ip, 0, sizeof(ifreq_ip));
	strncpy(ifreq_ip.ifr_name, ETH_IFACE, IFNAMSIZ-1);
	if (ioctl(fd_sock_up, SIOCGIFADDR, &ifreq_ip) < 0)
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
			//printf("ICMP downlink packet found\n");

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
			unsigned int res = sendto(fd_sock_up, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			//if (res < 0)
			//	printf("ICMP downlink packet send error\n");
			//else
			//	printf("ICMP downlink packet send success\n");
		}
		else if (ip->protocol == 6)		// TCP packet
		{
			//printf("TCP downlink packet found\n");
			
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
			struct pseudo_tcp_t *pseudo_tcp = (struct pseudo_tcp_t*)(tcpsumbuff);			
			pseudo_tcp->saddr = iph->saddr;
			pseudo_tcp->daddr = iph->daddr;
			pseudo_tcp->zero = 0;
			pseudo_tcp->protocol = IPPROTO_TCP;
			pseudo_tcp->len = htons(ntohs(ip->tot_len) - iphdrlen);
			total_len_sum += sizeof(struct pseudo_tcp_t);
			struct tcphdr *thsum = (struct tcphdr *)(tcpsumbuff + sizeof(struct pseudo_tcp_t));
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
			th->th_sum = checksumtcp((char*)(tcpsumbuff), sizeof(struct pseudo_tcp_t) + ntohs(ip->tot_len) - iphdrlen);
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

			unsigned int res = sendto(fd_sock_up, ethfrm_wr.data, ntohs(ip->tot_len) + 14,
					0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
			//if (res < 0)
			//	printf("TCP downlink packet send error\n");
			//else
			//	printf("TCP downlink packet send success\n");
		}
	}
}

// ### Functions ###############################################################
void ethfrm_set(ethfrm_t *ethfrm, uint8_t *data, uint16_t bytes)
{
	uint16_t i;

	ethfrm->bytes = bytes;
	for (i = 0; i < ethfrm->bytes; i++)
	{
		ethfrm->data[i] = data[i];
	}
}

void ethfrm_print(ethfrm_t ethfrm)
{
	uint16_t i;

	printf("Ethernet frame:\n");
	for (i = 0; i < ethfrm.bytes; i++)
		printf("%02X ", ethfrm.data[i]);
	printf("\n");
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
