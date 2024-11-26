/* EMACS_MODES: fill=0; c=1
 *
 * TRsniffer.c V0.5
 *
 *  Created on: Oct 17, 2012
 *      Author: pyoung (V0.1)
 *      	Sherwin (V0.2)
 *      	Ibu Akinyemi (V0.3)
 *      	Ibu Akinyemi (V0.4.1)
 *      	Ibu Akinyemi (V0.4.8)
 *      	Steve Ford (V0.5)
 */

#define STANDALONE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ether.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>

#include "lbmrpckt.h"
#include "TS_util.h"

/************************************
* BEGIN Constant definitions
************************************/
#define PCAP_MAGIC 0xa1b2c3d4
#define PCAP_MAJOR 2
#define PCAP_MINOR 4
#define PCAP_LINK_RAW 101 /* DLT_RAW */
#define MAX_RBUF  65536

#define MAX_PCAP_RECORD (sizeof(pcaprec_hdr_t) + MAX_RBUF)
#define ALIGN(addr) ((addr)+(addr%4))
#define BUF_OFFSET(b,off) (((char *)(b))+off)

#define MEGABYTE  (1024*1024)
#define DEFAULT_MAXBUF  10
#define DEFAULT_INTERFACE INADDR_ANY
#define DEFAULT_STATS_INTERVAL  30

#define PCAP_DEFAULT  "trsniff.pcap"

#define LBMR_WILDCARD_PATTERN_TYPE_PCRE 1
#define LBMR_WILDCARD_PATTERN_TYPE_REGEX 2
/*************************************
 * BEGIN Type definitions
 ************************************/

typedef struct sockInfo_s {
	int isMulti;
	struct sockaddr_in local;
	struct sockaddr_in remote;
	struct sockaddr_in interface;
	struct sockInfo_s *next;
	struct sockInfo_s *prev;
} sockInfo_t;

/* Program options */

typedef struct opt_s {
	uint16_t TR_port;
	uint32_t TR_group;
	uint32_t TR_interface;
	int raw;
	char *pcapFile;
	char *readpcapFile;
	char *statsFile;
	sockInfo_t *mcastAddrs; /* Linked list of multicast sockets */
	sockInfo_t *ucastAddrs; /* Linked list of unicast sockets */
	int interval; /* Stats interval in seconds */
	size_t maxBuf; /* Max buffering in MBytes */
	uint32_t stats_ivl;
	uint32_t clearstats_ivl;
	uint32_t uniquestatsonly;
	char stats;
	int found_lvl;
} opt_t;

/* PCAP file header */
typedef struct pcap_hdr_s {
	uint32_t magic_number; /* magic number */
	uint16_t version_major; /* major version number */
	uint16_t version_minor; /* minor version number */
	int32_t thiszone;  /* GMT to local correction */
	uint32_t sigfigs;  /* accuracy of timestamps */
	uint32_t snaplen;  /* max length of captured packets, in octets */
	uint32_t network;  /* data link type */
} pcap_hdr_t;

/* PCAP packet header */
typedef struct pcaprec_hdr_s {
	uint32_t ts_sec;   /* timestamp seconds */
	uint32_t ts_usec;  /* timestamp microseconds */
	uint32_t incl_len; /* number of octets of packet saved in file */
	uint32_t orig_len; /* actual length of packet */
} pcaprec_hdr_t;

typedef struct
{
	int fd;
	struct sockaddr_in local;
	struct sockaddr_in remote;
	int isMulti;
} fdinfo_t;

typedef struct addr_list {
	in_addr_t addr;
	uint16_t port;
	in_addr_t iface;
	int isMulti;
	int sockfd;
	struct addr_list *prev;
	struct addr_list *next;
} TRaddr_t;

/*************************************
 * BEGIN Forward Declarations
 ************************************/

int buildPcapRecord(struct sockaddr_in *sin, int recvSize, struct timeval *tv, TRaddr_t *addr);
//void initHeaders( struct iphdr *; struct udphdr *);
void initHeaders( void);
void pcapInitFile(int fd);
uint16_t cksum(int count, uint16_t *addr);

/*************************************
 * BEGIN Global data definitions
 ************************************/

char * USAGE=
        "[-h] [-m <group:port>] [-u <addr:port>] \n"
        "\t-m <group:port[:iface> - specify a multicast group and port and optional interface to listen:(default) 224.9.10.11:12965:0 \n"
        "\t-u <addr:port[:iface]> - specify an LBMRD address and port and optional interface \n"
        "\t-p <pcapfile> - PCAP Output filename \n"
        "\t-r <pcapfile> - Read from input PCAP filename \n"
        "\t\t-S <pcap#> - start processing from this packet in the PCAP file \n"
        "\t\t-E <pcap#> - stop processing at this packet in the PCAP file \n"
        "\t-v verbose output (Sets log mask to PRINT_ALL(0xFFFFFFFF) - (PRINT_DEBUG|PRINT_IP_FRAG|PRINT_PCAP|PRINT_DETAILS)) \n"
        "\t-l logging level - combine and specify log_mask in hex (defaults to PRINT_LBMR|PRINT_ERROR|PRINT_WARNING ):  \n"
        "\t                 -- PRINT_STATS:0x1,\t--PRINT_ERROR:0x2, \t--PRINT_WARNING:0x4, \t--PRINT_VERBOSE:0x8, \n"
        "\t                 -- PRINT_UMP:0x10 \t--PRINT_ULB:0x20, \t--PRINT_DRO:0x40, \t--PRINT_TMR:0x80, \n"
        "\t                 -- IP_FRAG:0x100, \t--PRINT_OTIDS:0x200, \t--PRINT_DEBUG:0x400, \t--PCAP:0x800 \n"
        "\t                 -- IP_DETAILS:0x1000, \t--PRINT_LBMR:0x2000, \t--PRINT_SOCKET:0x4000, \t--LBMRD:0x8000 \n"
        "\t                - Other options (e.g -v) may modify logging levels, so make this option last if you need to be specific \n"
        "\t-s <SEC> generate and dump accumulated stats for TIRs, TQRs, TMRs, OTIDs at every SEC interval. NOTE: adds PRINT_STATS to log_mask \n"
        "\t\t-C <SEC> clear accumulated stats generated by the '-s' option every SEC interval  \n"
        "\t\t-U <off_flag> Default is to accumulate only unique stats with '-s' option.Turn this off by setting off_flag!=0 \n"
	"\t-f change 'LBMR packet found' logs to log level PRINT_LBMR (most default to PRINT_STAT). \n"
	"\t-F <max_line*1000000>  - save output to a rolling log. Files are of ~max_line million lines in ./TR_LOGS dir. \n"
        "\t                    - File names start with 'x' \n"
        "\t-h prints this message";

char *optStr = "m:u:r:p:vs:fF:l:S:E:C:U:h";

struct option longopts[] = {
	{"multicast", 1, NULL, 'm'},
	{"unicast", 1, NULL, 'u'},
	{"readpcapfile", 1, NULL, 'r'},
	{"pcapfile", 1, NULL, 'p'},
	{"verbose", 0, NULL, 'v'},
	{"stats", 1, NULL, 's'},
	{"found_lbmr", 0, NULL, 'f'},
	{"file_stats", 1, NULL, 'F'},
	{"loglevel", 1, NULL, 'l'},
	{"startpkt", 1, NULL, 'S'},
	{"endpkt", 1, NULL, 'E'},
	{"clearstats", 1, NULL, 'C'},
	{"uniquestatsonly", 1, NULL, 'U'},
	{"help", 0, NULL, 'h'},
	{0}
};


#define LBM_RESOLVE_TIR_OPTS_COST_INFINITE (-1)
extern uint32_t TS_current_log_level;
extern uint32_t TS_max_fileline_count;
extern FILE *TS_output_file;



char * TS_tmr_topic_use = ": LBMR_TMR_TOPIC_USE ";
char * TS_tmr_leave_str = ": LBMR_TMR_LEAVE_TOPIC ";
char * TS_tmr_response_str = ": Response ";
char * TS_pcre_str = ",PCRE ";
char * TS_regex_str = ",REGEX ";


uint16_t TS_lbmrd_keepalive_received;
void TS_stats_OTID_print(void);
void TS_stats_TQRS_print(void);
void TS_stats_WTQRS_print(void);
void TS_stats_TMRS_print(void);
void TS_print_stored_stats(void);
void TS_clear_stored_stats(void);
void TS_stats_OTID(char *tname, char * str1, struct timeval *tv, char save_all_entries);
void TS_stats_TMR(char * tname, char * str1, struct timeval *tv, char save_all_entries);
void TS_stats_TQR(char * tname, char * str1, struct timeval *tv, char save_all_entries);
void TS_stats_WTQR(char *tname,char * str1, struct timeval *tv, char save_all_entries);
void TS_stats_DRO(char * tname, char *str1, struct timeval *tv, char save_all_entries);
void TS_stats_UMP(char * tname, char *str1, struct timeval *tv, char save_all_entries);
void TS_stats_ULB(char * tname, char *str1, struct timeval *tv, char save_all_entries);
TS_strings_listhead TS_otids_list;
TS_strings_listhead TS_tmrs_list;
TS_strings_listhead TS_tqrs_list;
TS_strings_listhead TS_wtqrs_list;
TS_strings_listhead TS_dros_list;
TS_strings_listhead TS_umps_list;
TS_strings_listhead TS_ulbs_list;

TRaddr_t *AddrList;
char *progname;
opt_t options;

/* DataBlock should be aligned correctly */
uint32_t DataBlock[(MAX_PCAP_RECORD/4)+1];
uint32_t tempDataBlock[(MAX_PCAP_RECORD/4)+1];

uint8_t *dataBuf = (uint8_t *)DataBlock;
uint8_t *temp_dataBuf = (uint8_t *)tempDataBlock;
pcaprec_hdr_t *pcapHdr;
struct iphdr *ipHdr;
struct udphdr *udpHdr;
struct ethhdr *ethHdr;
uint8_t *captureStart;
int outFd = -1;  // Default to stdout
int inFd = -1;  // Default to stdout

uint32_t stats_lbmr_pktCount = 0;
uint32_t stats_nonlbmr_pktCount =0;
uint32_t stats_otids =0;
uint32_t stats_tmrs =0;
uint32_t stats_tirs =0;
uint32_t stats_tqrs =0;
uint32_t stats_domains=0;
uint32_t stats_wtqrs =0;
uint32_t stats_umps =0;
uint32_t stats_ulbs =0;
uint32_t stats_dro_src =0;
uint32_t stats_ctxinfo =0;
uint32_t stats_lbmrd_keepalive =0;
uint32_t TS_save_all_entries = 0;
int prev_num_domains=-1;

bool unicastTR=false;
bool mcastTR=false;
struct frag_id {
	uint16_t id;
	int frag_offset;
	int nBytes;
	char *data;
	struct frag_id *next;
} ;

struct frags_list {
	 uint16_t id;
	 char incomplete_flag;
	 char got_last_frag;
	 struct frag_id *frag;
	 struct frags_list *next;
	 char frags_received;
	 char * reassembled_msg;
};
struct frags_list list_head ;
/*************************************
 * BEGIN Functions
 ************************************/
void TSdump(uint32_t level, const char *buffer, int size);
struct frags_list * TS_add_ip_fragment (uint16_t id, struct frag_id *ip_frag, uint16_t no_more_fragments );
int TS_assemble_udp_frags(uint16_t id, unsigned char * buff_ptr, int nBytes, int frag_offset, uint16_t  mf_flag, struct timeval tv, struct sockaddr_in *fromaddr);
int TS_reassmble_ip_fragment (uint16_t id, struct frags_list *prev_list, int msg_size);


void Usage(char * progname)
{
	fprintf(stderr, "%s %s\n", progname, USAGE);
}

void TS_pretty_print_addr( uint32_t level, uint32_t ipaddr){
	TS_printf(level, "(%i.%i.%i.%i)",((ipaddr >> 24) & 0xFF), ((ipaddr >> 16) & 0xFF), ((ipaddr >> 8) & 0xFF), ((ipaddr) & 0xFF));
}

int bindUniSocket(TRaddr_t *uaddr) {
	struct sockaddr_in uniAddr;
	int status;
	
	memset((char *)&uniAddr, 0, sizeof(uniAddr));

	uniAddr.sin_family = AF_INET;
	uniAddr.sin_addr.s_addr = uaddr->iface;

	/* client end of LBMRD is always an ephemeral port */
	uniAddr.sin_port = htons(0);

	status= bind( uaddr->sockfd, (struct sockaddr *)&uniAddr, sizeof(uniAddr));

	TS_printf(PRINT_SOCKET, "bindUniSocket - sockfd = %d\n", uaddr->sockfd);

	if (status < 0) {
		struct sockaddr_in anyaddr;
		perror("bind(ifaceaddr)");
		fprintf(stderr, "Retrying with INADDR_ANY - ret=%d\n", status);
		memcpy((void *)&anyaddr, (void *)&uniAddr, sizeof(anyaddr));
		anyaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		status = bind(uaddr->sockfd, (struct sockaddr *)&anyaddr, sizeof(anyaddr));
		if (status < 0) {
			perror("bind(anyaddr)");
			exit(errno);
		}
	}
	return 0;
}

int
bindMultiSocket(TRaddr_t *maddr)
{
	struct sockaddr_in groupaddr;
	struct ip_mreq memreq;
	int status;
	memset((char *)&groupaddr, 0, sizeof(groupaddr));
	groupaddr.sin_family = AF_INET;
	groupaddr.sin_addr.s_addr = maddr->addr;
	groupaddr.sin_port = maddr->port;

	/* Bind to the MCAST group */
	status = bind(maddr->sockfd, (struct sockaddr *)&groupaddr, sizeof(groupaddr));
	if (status < 0) {
		struct sockaddr_in anyaddr;
		perror("bind(groupaddr)");
		fprintf(stderr, "Retrying with INADDR_ANY\n");
		TS_printf(PRINT_WARNING|PRINT_SOCKET, "\n ERROR! bind(groupaddr): Retrying with INADDR_ANY ");
		memcpy((void *)&anyaddr, (void *)&groupaddr, sizeof(anyaddr));
		anyaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		status = bind(maddr->sockfd, (struct sockaddr *)&anyaddr, sizeof(anyaddr));
		if (status < 0) {
			TS_printf(PRINT_ERROR|PRINT_SOCKET, "\n ERROR! bind(anyaddr) ");
			perror("bind(anyaddr)");
			exit(errno);
		}
	}

	memset((void *)&memreq, 0, sizeof(memreq));
	memreq.imr_multiaddr.s_addr = maddr->addr;
	memreq.imr_interface.s_addr = maddr->iface;

	/* Join the MCAST group */
	status = setsockopt(maddr->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&memreq, sizeof(memreq));
	if (status < 0) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		exit(errno);
	}
	return status;
}

int
openSocket(TRaddr_t *addr)
{
	int status;
	int one = 1;

	captureStart = (uint8_t *)BUF_OFFSET(udpHdr, sizeof(*udpHdr));
	addr->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (addr->sockfd < 0) {
		TS_printf(PRINT_ERROR|PRINT_SOCKET, "\n ERROR! SOCK_DGRAM");
		perror("socket(SOCK_DGRAM)");
		exit(errno);
	}

	status = setsockopt(addr->sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one));
	if (status < 0) {
		TS_printf(PRINT_ERROR|PRINT_SOCKET, "\n ERROR! SO_REUSEADDR");
		perror("setsockopt(SO_REUSEADDR)");
		exit(errno);
	}

	if (addr->isMulti) {
		status = bindMultiSocket(addr);
	} else {
		status = bindUniSocket(addr);
	}
	return status;
}

int
addAddrList(char *addr, char *port, char *iface, int isMulti)
{
	TRaddr_t *new;
	TRaddr_t *old;
	char *end;
	int status;

	new = (TRaddr_t *)malloc(sizeof(TRaddr_t));
	memset(new, 0, sizeof(TRaddr_t));
	new->sockfd = -1;
	new->addr = inet_addr(addr);
	new->port = (uint16_t)strtol(port, &end, 10);
	if (end == port) {
		return -1;
	}

	new->port = htons(new->port);

	new->isMulti = isMulti;
	new->iface = inet_addr(iface);

	TS_printf(PRINT_SOCKET,"\naddAddrList( IP=%s port=%s iface=%s isMulti=%i)", addr, port, iface, isMulti );
	TS_printf(PRINT_DEBUG,"\nadding ip=0x%x port=%d iface=0x%x\n", ntohl(new->addr), ntohs(new->port),
	        ntohl(new->iface));
	
	status = openSocket(new);
	if (status != 0) {
		TS_printf(PRINT_ERROR|PRINT_SOCKET, "\n ERROR! openSocket: status %i", status);
		return -1;
	}

	if (AddrList == NULL) {
		AddrList = new;
		new->prev = new;
		new->next = new;
	} else {
		old = AddrList->prev;
		AddrList->prev = new;
		new->prev = old;
		new->next = old->next;
		old->next = new;
	}
	return 0;
}




int
DecodeLbmrPacket(uint8_t *pktData, struct sockaddr_in *fromaddr, int recvSize, struct timeval *tv, TRaddr_t *addrList)
{
	uint8_t *ptr = pktData;
	lbmr_hdr_t *lbm_hdr_ptr;
	lbmr_remote_domain_route_hdr_t * route_hdr_ptr;
	lbmr_lbmr_opt_len_t * lbmr_lbmr_hdr_ptr;
	char * opt_ptr;
	int bytes_to_parse = 0;
	int opt_bytes_to_parse = 0;
	lbm_uint16_t optflags;
	lbmr_lbmr_opt_hdr_t *opthdr = NULL;
	lbmr_lbmr_opt_src_type_t *optsrctype = NULL;
	lbmr_lbmr_opt_src_id_t *optsrcid = NULL;
	lbmr_lbmr_opt_version_t *opt_version = NULL;
	lbm_uint32_t msg_domain_id;
	lbmr_lbmr_opt_local_domain_t *opt_local_domain = NULL;
	lbmr_hdr_ext_type_t *ext_type_ptr;
	lbmr_topic_opt_len_t *topic_opt_ptr;
	lbmr_topic_opt_t *t_opthdr_ptr;
	lbmr_tir_t *tir_ptr;
	lbmr_topic_opt_otid_t *opt_oid_ptr ;
	lbmr_topic_opt_cost_t *opt_cost_ptr;
	lbmr_topic_opt_ulb_t *opt_ulb_ptr;
	lbmr_topic_opt_exfunc_t *opt_exfunc_ptr;
	lbmr_topic_opt_ctxinst_t * opt_ctxinst_ptr;
	lbmr_topic_opt_domain_id_t * topic_opt_domain_id_ptr;
	lbmr_tir_tcp_with_sid_t *tir_tcp_with_sid_ptr;
	lbmr_tir_tcp_t *tir_tcp_ptr;
	lbmr_tir_lbtrm_t *tir_lbtrm_ptr;
	lbmr_tir_lbtru_with_sid_t *tir_lbtru_with_sid_ptr;
	lbmr_tir_lbtru_t  *tir_lbtru_ptr;
	lbmr_tir_lbtipc_t *tir_lbtipc_ptr;
	lbmr_tir_lbtsmx_t *tir_lbtsmx_ptr;
	lbmr_tir_lbtrdma_t *tir_lbtrdma_ptr;
	lbmr_topic_opt_ume_t *opt_ume_ptr;
	lbmr_topic_opt_latejoin_t *lj_opt_ptr;
	lbmr_tmb_t * tmb_ptr;
	lbmr_tmr_t * tmr_ptr;
	lbmr_topic_res_request_t * topic_res_request_ptr;
	lbmr_rctxinfo_t * lbmr_rctxinfo_ptr;
        lbm_transport_source_info_t tinfo;
	int lenparsed, ctr;
	char app_maj_ver;
	
        char options_str[TS_MAX_STRING_LENGTH];
        char source_name[TS_MAX_STRING_LENGTH];
        size_t source_size = sizeof(source_name);
	char tname[TS_MAX_STRING_LENGTH];
	char temp_buff[TS_MAX_STRING_LENGTH];
	char ex_func_str[50];
	uint32_t cnt, cnt2, num_tirs;
	int tir_cnt;
	int tqr_cnt;
	int slen;
	int opt_total_len;
	char otid_flag = 0;
	char dro_src_flag = 0;
	char ex_func_flag = 0;
	
	
	char *str2_ptr;

	/* Initialize strings */
	options_str[0] = '\0';
	source_name[0] = '\0';
	tname[0] = '\0';
	temp_buff[0] = '\0';
	ex_func_str[0] = '\0';

	lbm_hdr_ptr = (lbmr_hdr_t *) pktData;
	/* Check valid packets: buffer length , type, options length, etc */
	switch (LBMR_HDR_VER(lbm_hdr_ptr->ver_type)) {
	        case LBMR_VERSION_0:
			TS_printf(PRINT_VERBOSE, "\n LBMR Version 0 detected ");
			break;
        	case LBMR_VERSION_1:
			TS_printf(PRINT_VERBOSE, "\n LBMR Version 1 detected ");
               		break;
        	default:
			/* Not an LBMR candidate */
			return 1;
	}

	otid_flag = 0;
	dro_src_flag = 0;
	/* list_head.id = 0xFFFF; .. protect against 0 ID 4.6 .. note that 0xFFFF may also be a problem. */

	if( options.stats == 1 ){
		snprintf(options_str, LBM_MSG_MAX_SOURCE_LEN, ",");
	}

	if ((lbm_hdr_ptr->ver_type & LBMR_HDR_TYPE_OPTS_MASK) == LBMR_HDR_TYPE_OPTS_MASK){
		/* Sanity checks */
		/****************/

		/* If the length is less than minimum, not LBMR packet */
		if ( (recvSize) < (LBMR_HDR_SZ + LBMR_LBMR_OPT_LEN_SZ)) {
			TS_printf(PRINT_VERBOSE, "\n recvSize %x < (LBMR_HDR_SZ + LBMR_LBMR_OPT_LEN_SZ) %x not valid LBMR", recvSize, (LBMR_HDR_SZ + LBMR_LBMR_OPT_LEN_SZ) );
			return 2;
		}
		lbmr_lbmr_hdr_ptr = (lbmr_lbmr_opt_len_t *) &ptr[recvSize - LBMR_LBMR_OPT_LEN_SZ];
		if ( lbmr_lbmr_hdr_ptr->type != LBMR_LBMR_OPT_LEN_TYPE ) {
			TS_printf(PRINT_ERROR, "\n error lbmr_lbmr_hdr_ptr->type %x, expect %x ", lbmr_lbmr_hdr_ptr->type, LBMR_LBMR_OPT_LEN_TYPE );
				return 3;
		}
		if ( lbmr_lbmr_hdr_ptr->len != LBMR_LBMR_OPT_LEN_SZ ) {
			TS_printf(PRINT_ERROR, "\n error lbmr_lbmr_hdr_ptr->len %x expect %x", lbmr_lbmr_hdr_ptr->len, LBMR_LBMR_OPT_LEN_SZ );
				return 4;
		}
		opt_total_len =  ntohs(lbmr_lbmr_hdr_ptr->total_len);
		if ( opt_total_len < LBMR_LBMR_OPT_LEN_SZ ) {
			TS_printf(PRINT_ERROR, "\n error lbmr_lbmr_hdr_ptr->total_len %i should be > %i", opt_total_len, LBMR_LBMR_OPT_LEN_SZ );
				return 5;
		}

		/*  Go through options fields and extract the data */
		/****************************************************/
		opt_bytes_to_parse = ntohs(lbmr_lbmr_hdr_ptr->total_len);
		opt_ptr = (char *)pktData + (recvSize - opt_total_len);

		while (opt_bytes_to_parse > 0) {
			opthdr = (lbmr_lbmr_opt_hdr_t *) opt_ptr;
			optflags = ntohs(opthdr->flags);

			TS_printf(PRINT_DEBUG, "\n opthdr->type = %x",  opthdr->type);
			switch (opthdr->type){
		   	  case LBMR_LBMR_OPT_LEN_TYPE:
				TS_printf(PRINT_VERBOSE, "\n LBMR_LBMR_OPT_LEN_TYPE ");
				TS_printf(PRINT_DEBUG, "\n done options parsing!");
			  break;
			  case LBMR_LBMR_OPT_SRC_TYPE_TYPE:
				TS_printf(PRINT_VERBOSE, "\n LBMR_LBMR_OPT_SRC_TYPE_TYPE ");
				optsrctype = (lbmr_lbmr_opt_src_type_t *) opt_ptr;
				if (optsrctype->len != LBMR_LBMR_OPT_SRC_TYPE_SZ) {
					TS_printf(PRINT_ERROR, "\n ERROR! LBMR src type option invalid len: %i, Expected %i", optsrctype->len, LBMR_LBMR_OPT_SRC_TYPE_SZ);
					return 110;
				}
				if (optsrctype->src_type == 1){
					TS_printf(PRINT_VERBOSE, "\n DRO Version");
					if( options.stats == 1 ){
						strncat(options_str,",DRO_V", LBM_MSG_MAX_SOURCE_LEN );
					}
				} else {
					TS_printf(PRINT_VERBOSE, "\n Application Version: ");
					if( options.stats == 1 ){
						strncat(options_str,",App_V", LBM_MSG_MAX_SOURCE_LEN );
					}
				}
			  break;
			  case LBMR_LBMR_OPT_SRC_ID_TYPE:
				TS_printf(PRINT_VERBOSE, "\n LBMR_LBMR_OPT_SRC_ID_TYPE: ");
				optsrcid = (lbmr_lbmr_opt_src_id_t *) opt_ptr;
				if (optsrcid->len != LBMR_LBMR_OPT_SRC_ID_SZ) {
					TS_printf(PRINT_ERROR, "\n ERROR! LBMR src ID option invalid len: %x expecting %x", optsrcid->len, LBMR_LBMR_OPT_SRC_ID_SZ);
					 return 111;
				}
				{
					sprintf(temp_buff,"CtxID:0x%02x%02x%02x%02x%02x%02x%02x%02x",* ((unsigned char *) optsrcid->src_id + 0),  *((unsigned char *) optsrcid->src_id + 1), *((unsigned char *) optsrcid->src_id + 2), *((unsigned char *) optsrcid->src_id + 3), *((unsigned char *) optsrcid->src_id + 4), *((unsigned char *) optsrcid->src_id + 5), *((unsigned char *) optsrcid->src_id + 6), *((unsigned char *) optsrcid->src_id + 7));
					TS_printf(PRINT_VERBOSE, "\n Context %s",temp_buff);
					if( options.stats == 1 ){
						strncat(options_str, temp_buff, LBM_MSG_MAX_SOURCE_LEN );
					}
				}
					
			  break;
			  case LBMR_LBMR_OPT_VERSION_TYPE:
			  {
				uint32_t opt_ver_buf;
				TS_printf(PRINT_DEBUG, "\n LBMR_LBMR_OPT_VERSION_TYPE ");
				opt_version = (lbmr_lbmr_opt_version_t *) opt_ptr;
				
				opt_ver_buf =  ntohl(opt_version->version);
				sprintf(temp_buff, ":%d.%d.%d.%d%s%s",
                                                                ((opt_ver_buf >> 24) & 0xFF),
                                                                ((opt_ver_buf >> 16) & 0xFF),
                                                                ((opt_ver_buf >> 8) & 0xFF),
                                                                (opt_ver_buf & 0xFF),
                                                                (optflags & LBMR_LBMR_OPT_VERSION_FLAG_UME ? "(UMP)" : ""),
                                                                (optflags & LBMR_LBMR_OPT_VERSION_FLAG_UMQ ? "(UMQ)" : ""));
				app_maj_ver = ((opt_ver_buf >> 24) & 0xFF);
				TS_printf(PRINT_VERBOSE, "LBMR product %s, Major Version: %i", temp_buff, app_maj_ver);
				if( options.stats == 1 ){
					strncat(options_str,temp_buff, LBM_MSG_MAX_SOURCE_LEN );
				}
			  }
			  break;
			  case LBMR_LBMR_OPT_LOCAL_DOMAIN_TYPE:
				TS_printf(PRINT_DRO, "\n LBMR_LBMR_OPT_LOCAL_DOMAIN_TYPE ");
				opt_local_domain = (lbmr_lbmr_opt_local_domain_t *) opt_ptr;
				if (opt_local_domain->len != LBMR_LBMR_OPT_LOCAL_DOMAIN_SZ) {
				        TS_printf(PRINT_ERROR, "\n LBMR Domain ID option invalid len %x expecting %x ", opt_local_domain->len, LBMR_LBMR_OPT_LOCAL_DOMAIN_SZ);
					return 120;
				}
				msg_domain_id = ntohl(opt_local_domain->local_domain_id);
				sprintf(temp_buff, ",Msg_domain_id:%i", msg_domain_id);
				TS_printf(PRINT_DRO, "%s", temp_buff);
				if( options.stats == 1 ){
					strncat(options_str, temp_buff, LBM_MSG_MAX_SOURCE_LEN );
				}
				
			  break;
			  default:
				if ((optflags & LBMR_LBMR_OPT_HDR_FLAG_IGNORE) == 0) {
					TS_printf(PRINT_WARNING, "\n WARNING! invalid option type ... <to do>: upgrade to error? ");
					/* return 100; */
				}
				TS_printf(PRINT_WARNING, "\n WARNING: No option found ");
			   break;
			}
			if(opthdr->len == 0) {
				TS_printf(PRINT_ERROR, "\n  ERROR: Invalid 0-length option field. ");
				return 112;
			}
			opt_ptr += opthdr->len;
			opt_bytes_to_parse -= opthdr->len;
			TS_printf(PRINT_DEBUG, "\n opt_bytes_to_parse  = %i", opt_bytes_to_parse);
		}/* End while */


	} /* End if opts */

	/* to do: improve sanity check to see what type of lbmrs can be batched together and if the total length should be checked in a for loop */
	/* so, update bytes_to_parse as you cycle thru right now, make ass-umtions  */

	/* What type of LBMR? */
	bytes_to_parse = recvSize - LBMR_HDR_SZ;

	num_tirs = ntohs(lbm_hdr_ptr->tirs);
	switch (LBMR_HDR_TYPE(lbm_hdr_ptr->ver_type)){
	  case LBMR_HDR_TYPE_WC_TQRS:
		ptr = pktData + LBMR_HDR_SZ;
		TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_TYPE_WC_TQRS");
	    	if( *ptr == LBMR_WILDCARD_PATTERN_TYPE_REGEX){
			str2_ptr = TS_regex_str;
			TS_printf(PRINT_VERBOSE, "\n LBMR_WILDCARD_PATTERN_TYPE_REGEX ");
	    	} else if( *ptr == LBMR_WILDCARD_PATTERN_TYPE_PCRE){
			str2_ptr = TS_pcre_str;
			TS_printf(PRINT_VERBOSE, "\n LBMR_WILDCARD_PATTERN_TYPE_PCRE ");
	    	} else {
			TS_printf(PRINT_ERROR, "\n ERROR! Unrecognized WC type");
			return 200;
	    	}
		tname[LBM_MAX_TOPIC_NAME_LEN+11]=0;
		for( tqr_cnt = 0; tqr_cnt < lbm_hdr_ptr->tqrs; tqr_cnt++){
			snprintf(tname, LBM_MAX_TOPIC_NAME_LEN+11,"WC_PATTERN:%s", (ptr+1));
			if (tname[LBM_MAX_TOPIC_NAME_LEN+11] != 0) {
				TS_printf(PRINT_ERROR, "\n ERROR! topic name too large ");
				return 210;
			}
			TS_printf(PRINT_VERBOSE, " \t WC Pattern: %s ", tname);
			stats_wtqrs++;
			if( options.stats == 1 ){
				cnt = ntohl(fromaddr->sin_addr.s_addr);
				snprintf(source_name,LBM_MSG_MAX_SOURCE_LEN-1, "%s[%i]:%i.%i.%i.%i:%i",tname,msg_domain_id,((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),ntohs(fromaddr->sin_port));
				TS_stats_WTQR(source_name, strcat(options_str,str2_ptr), tv, TS_save_all_entries);
			}
			/* substract off the 10 characters for "WC_PATTERN:" added to topic pattern */
			slen = (strlen(tname) + 1) - 10;
			bytes_to_parse -= slen;
			ptr += slen;
		}
	  break;
	  case LBMR_HDR_TYPE_NORMAL:
		if ((lbm_hdr_ptr->tqrs==0)  && (num_tirs==0)) {
			if( unicastTR == true ) {
				TS_printf(PRINT_LBMRD|PRINT_VERBOSE,"\n\tLBMRD Keepalive[%i] response at %i.%i", TS_lbmrd_keepalive_received, tv->tv_sec, tv->tv_usec);
				TS_lbmrd_keepalive_received--;
				stats_lbmrd_keepalive++;
				return 99999;
			} else {
				TS_printf(PRINT_LBMRD, "\n LBMRD Keepalive ");
			}
		}
		if (num_tirs>1000) {
			/* To do: Experiments show 800+ with resolver datagram set to MAX. I can do better by checking packet length and diving by the header. */
			TS_printf(PRINT_ERROR, "\n TIRs greater than 1000 Likely invalid ");
			return 316;
		}
		if (lbm_hdr_ptr->tqrs>250) {
			/* To do: This probably also needs another look */
			TS_printf(PRINT_ERROR, "\n TQRs greater than 250 Likely invalid ");
			return 317;
		}
		TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_TYPE_NORMAL");
		TS_printf(PRINT_LBMR|PRINT_VERBOSE, "\n ********************* TQRs %u TIRs %u [len %u] **************************",lbm_hdr_ptr->tqrs, num_tirs, bytes_to_parse);
		ptr = pktData + LBMR_HDR_SZ;
		tname[LBM_MAX_TOPIC_NAME_LEN] = 0;
		{ /* Parse TQRs */
			for( tqr_cnt = 0; tqr_cnt < lbm_hdr_ptr->tqrs; tqr_cnt++){
				strncpy(tname, (char *)ptr, LBM_MAX_TOPIC_NAME_LEN);
				if (tname[LBM_MAX_TOPIC_NAME_LEN] != 0) {
					TS_printf(PRINT_ERROR, "\n ERROR! topic name too large ");
					return 330;
				}
				stats_tqrs++;
				TS_printf(PRINT_VERBOSE, "\n TQR[#%i]  Topic Name: %s ", stats_tqrs, tname);
				TS_printf(PRINT_VERBOSE, "\n TQR: options string %s ", options_str);
				if( options.stats == 1 ){
					cnt = ntohl(fromaddr->sin_addr.s_addr);
					TS_printf(PRINT_LBMR, "\n TQR from " ); TS_pretty_print_addr(PRINT_LBMR, fromaddr->sin_addr.s_addr);
					snprintf(source_name,LBM_MSG_MAX_SOURCE_LEN-1, "%s[%i],%i.%i.%i.%i:%i",tname, msg_domain_id,
						((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),ntohs(fromaddr->sin_port));
					TS_stats_TQR(source_name, options_str, tv, TS_save_all_entries);
				}
				slen = strlen(tname) + 1;
				bytes_to_parse -= slen;
				ptr += slen;
			}
		} /* End parse TQRs */

		{ /* Parse TIRs */
			for( tir_cnt = 0; tir_cnt < num_tirs; tir_cnt++){

				strncpy(tname, (char *)ptr, LBM_MAX_TOPIC_NAME_LEN);
				if (tname[LBM_MAX_TOPIC_NAME_LEN] != 0) {
					TS_printf(PRINT_ERROR, "\n ERROR! topic name too large ");
					return 400;
				}
				TS_printf(PRINT_VERBOSE, "\n\n[TIR #%i] Topic Name: %s ", stats_tirs+1, tname);
				slen = strlen(tname) + 1;
				bytes_to_parse -= slen;
				ptr += slen;
				tir_ptr = (lbmr_tir_t *) ptr;

				TS_printf(PRINT_DEBUG, "\n tir_ptr->transport.optionspresent %x",  (tir_ptr->transport  & LBMR_TIR_OPTIONS) >> 7);
				TS_printf(PRINT_DEBUG, "\n tir_ptr->transport %x",  tir_ptr->transport  & LBMR_TIR_TRANSPORT );
				TS_printf(PRINT_DEBUG, "\n tir_ptr->tlen %i",  tir_ptr->tlen );
				TS_printf(PRINT_DEBUG, "\n tir_ptr->ttl %i", ntohs(tir_ptr->ttl) );
				TS_printf(PRINT_VERBOSE, "\n tir_ptr->index %x", ntohl(tir_ptr->index));
				/* Parse through the options */
				if ( (tir_ptr->transport & LBMR_TIR_OPTIONS) == LBMR_TIR_OPTIONS){
				  ptr+=LBMR_TIR_SZ;
				  bytes_to_parse -= LBMR_TIR_SZ;
				  topic_opt_ptr = (lbmr_topic_opt_len_t *) ptr;
				  /* Sanity checks */
				  if( topic_opt_ptr->type != LBMR_TOPIC_OPT_LEN_TYPE){
					TS_printf(PRINT_ERROR, "\n ERROR: wrong LBMR_TOPIC_OPT_LEN_TYPE %x, expecting %x ", topic_opt_ptr->type, LBMR_TOPIC_OPT_LEN_TYPE);
					return 401;
				  }
				  if( topic_opt_ptr->len != LBMR_TOPIC_OPT_LEN_SZ){
					TS_printf(PRINT_ERROR, "\n ERROR: wrong LBMR_TOPIC_OPT_LEN_SZ %x, expecting %x ", topic_opt_ptr->len, LBMR_TOPIC_OPT_LEN_SZ);
					return 402;
				  }
				  opt_total_len = ntohs(topic_opt_ptr->total_len);
				  TS_printf(PRINT_DEBUG, "\n topic_opt_ptr->total_len = %i ", opt_total_len );
				  if ( opt_total_len > bytes_to_parse ) {
					 TS_printf(PRINT_ERROR, "\n ERROR: opt_total_len %i > bytes_to_parse %i", opt_total_len, bytes_to_parse);
					 return 403;
				  }

				  opt_bytes_to_parse = opt_total_len - topic_opt_ptr->len;
				  bytes_to_parse -=  opt_bytes_to_parse;
				  ptr += LBMR_TOPIC_OPT_LEN_SZ;
				  while ( opt_bytes_to_parse > 0 ){
					t_opthdr_ptr = (lbmr_topic_opt_t *) ptr;

					TS_printf(PRINT_DEBUG, "\n OPTION = ");
					for (cnt = 0; cnt < t_opthdr_ptr->len; cnt++){
						TS_printf(PRINT_DEBUG, " %x ", ptr[cnt]);
					}

					if( t_opthdr_ptr->len == 0 ){
						TS_printf(PRINT_ERROR, "\n ERROR! t_opthdr_ptr->len = 0 ");
						return 500;
					}
					switch(t_opthdr_ptr->type){
					lbmr_topic_opt_ume_t opt_ume_buf;

						case LBMR_TOPIC_OPT_UME_TYPE:
							opt_ume_ptr = (lbmr_topic_opt_ume_t *)ptr;
							 ptr += LBMR_TOPIC_OPT_UME_SZ;
							if( opt_ume_ptr->len != LBMR_TOPIC_OPT_UME_SZ) {
								TS_printf(PRINT_ERROR, "\n ERROR! opt_ume_ptr->len != LBMR_TOPIC_OPT_UME_SZ" );
								return 501;
							}
							if(opt_ume_ptr->store_tcp_port == 0) {
								TS_printf(PRINT_WARNING, "\n  WARNING! Check valid. UME Store info port is set to zero .. will continue ");
							}
							if(opt_ume_ptr->src_tcp_port == 0) {
								TS_printf(PRINT_WARNING, "\n  WARNING! Check valid. umestore selected but source request bind port set to 0... will continue ");
							}

							/* Host conversion */
							opt_ume_buf.flags=ntohs(opt_ume_ptr->flags);
							opt_ume_buf.store_tcp_port=ntohs(opt_ume_ptr->store_tcp_port);
							opt_ume_buf.src_tcp_port=ntohs(opt_ume_ptr->src_tcp_port);
							opt_ume_buf.store_tcp_addr=ntohl(opt_ume_ptr->store_tcp_addr);
							opt_ume_buf.src_tcp_addr=ntohl(opt_ume_ptr->src_tcp_addr);
							opt_ume_buf.src_reg_id=ntohl(opt_ume_ptr->src_reg_id);
							opt_ume_buf.transport_idx=ntohl(opt_ume_ptr->transport_idx);
							opt_ume_buf.high_seqnum=ntohl(opt_ume_ptr->high_seqnum);
							opt_ume_buf.low_seqnum=ntohl(opt_ume_ptr->low_seqnum);

							TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_UME_TYPE ");
							TS_printf(PRINT_VERBOSE, "\n UME FLAGS: %x", opt_ume_buf.flags);
							if ((opt_ume_buf.flags & LBMR_TOPIC_OPT_UME_FLAG_IGNORE) == LBMR_TOPIC_OPT_UME_FLAG_IGNORE){
								TS_printf(PRINT_VERBOSE|PRINT_UMP, " UME_FLAG_IGNORE");
							}
							if ((opt_ume_buf.flags & LBMR_TOPIC_OPT_UME_FLAG_LATEJOIN) == LBMR_TOPIC_OPT_UME_FLAG_LATEJOIN){
								TS_printf(PRINT_VERBOSE|PRINT_UMP, " UME_FLAG_LATEJOIN");
							}
							if ((opt_ume_buf.flags & LBMR_TOPIC_OPT_UME_FLAG_STORE) == LBMR_TOPIC_OPT_UME_FLAG_STORE){
								TS_printf(PRINT_VERBOSE|PRINT_UMP, " UME_FLAG_STORE");
							}
							if ((opt_ume_buf.flags & LBMR_TOPIC_OPT_UME_FLAG_QCCAP) == LBMR_TOPIC_OPT_UME_FLAG_QCCAP){
								TS_printf(PRINT_VERBOSE|PRINT_UMP, " UME_FLAG_QCCAP");
							}
							if ((opt_ume_buf.flags & LBMR_TOPIC_OPT_UME_FLAG_ACKTOSRC) == LBMR_TOPIC_OPT_UME_FLAG_ACKTOSRC){
								TS_printf(PRINT_VERBOSE|PRINT_UMP, " UME_FLAG_ACKTOSRC");
							}
							if ((opt_ume_buf.flags & LBMR_TOPIC_OPT_UME_FLAG_CTXID) == LBMR_TOPIC_OPT_UME_FLAG_CTXID){
								TS_printf(PRINT_VERBOSE|PRINT_UMP, " UME_FLAG_CTXID");
							}

							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n store_tcp_port = %i ", opt_ume_buf.store_tcp_port);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n src_tcp_port = %i ", opt_ume_buf.src_tcp_port);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n store_tcp_addr = %i ", opt_ume_buf.store_tcp_addr);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n src_tcp_addr = %i ", opt_ume_buf.src_tcp_addr);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n src_reg_id = %i ", opt_ume_buf.src_reg_id);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n transport_idx = %i ", opt_ume_buf.transport_idx);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n high_seqnum = %i ", opt_ume_buf.high_seqnum);
							TS_printf(PRINT_VERBOSE|PRINT_UMP, "\n low_seqnum = %i ", opt_ume_buf.low_seqnum);

							ptr += LBMR_TOPIC_OPT_UME_SZ;
							opt_bytes_to_parse -= LBMR_TOPIC_OPT_UME_SZ;
						break;
						case LBMR_TOPIC_OPT_UMQ_QINFO_TYPE:
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_UMQ_QINFO_TYPE ");
							 TS_printf(PRINT_ERROR, ":ERROR!: not yet implemented:"); return -1;
						break;
						case LBMR_TOPIC_OPT_LATEJOIN_TYPE:
						{
							lbmr_topic_opt_latejoin_t lj_opt_buf;
							TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_LATEJOIN_TYPE ");
							lj_opt_ptr = (lbmr_topic_opt_latejoin_t *) ptr;
							lj_opt_buf.flags=ntohs(lj_opt_ptr->flags);
							lj_opt_buf.src_tcp_port=ntohs(lj_opt_ptr->src_tcp_port);
							lj_opt_buf.src_ip_addr=ntohl(lj_opt_ptr->src_ip_addr);
							lj_opt_buf.transport_idx=ntohl(lj_opt_ptr->transport_idx);
							lj_opt_buf.high_seqnum=ntohl(lj_opt_ptr->high_seqnum);
							lj_opt_buf.low_seqnum=ntohl(lj_opt_ptr->low_seqnum);

							TS_printf(PRINT_DETAILS|PRINT_UMP, "\n lj_opt_buf.flags = %x", lj_opt_buf.flags);
							TS_printf(PRINT_DETAILS|PRINT_UMP, "\n lj_opt_buf.src_tcp_port = %x", lj_opt_buf.src_tcp_port);
							TS_printf(PRINT_DETAILS|PRINT_UMP, "\n lj_opt_buf.src_ip_addr = %x", lj_opt_buf.src_ip_addr);
							TS_printf(PRINT_DETAILS|PRINT_UMP, "\n lj_opt_buf.transport_idx = %x", lj_opt_buf.transport_idx);
							TS_printf(PRINT_DETAILS|PRINT_UMP, "\n lj_opt_buf.high_seqnum = %x", lj_opt_buf.high_seqnum);
							TS_printf(PRINT_DETAILS|PRINT_UMP, "\n lj_opt_buf.low_seqnum = %x", lj_opt_buf.low_seqnum);
							ptr += LBMR_TOPIC_OPT_LATEJOIN_SZ;
							opt_bytes_to_parse -= LBMR_TOPIC_OPT_LATEJOIN_SZ;
							if( options.stats == 1 ){
								char lj_source_name[300];
								cnt = lj_opt_buf.src_ip_addr;
								snprintf(lj_source_name,LBM_MSG_MAX_SOURCE_LEN-1,
									 "LATE_JOIN:INFO,%s[%i],%i.%i.%i.%i:%i,transport_idx:%x,high_seqnum:%i,low_seqnum:%i",
									 tname, msg_domain_id,
									 ((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
									lj_opt_buf.src_tcp_port,lj_opt_buf.transport_idx,lj_opt_buf.high_seqnum,lj_opt_buf.low_seqnum);
								TS_stats_UMP( lj_source_name, options_str, tv, TS_save_all_entries );
							}
						}
						break;
						case LBMR_TOPIC_OPT_UME_STORE_TYPE:
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_UME_STORE_TYPE: ... NOT IMPLEMENTED ");
							 ptr += LBMR_TOPIC_OPT_UME_STORE_SZ;
							 opt_bytes_to_parse -= LBMR_TOPIC_OPT_UME_STORE_SZ;
						break;
						case LBMR_TOPIC_OPT_UME_STORE_GROUP_TYPE:
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_UME_STORE_GROUP_TYPE:  ");
							 ptr += LBMR_TOPIC_OPT_UME_STORE_GROUP_SZ;
							 opt_bytes_to_parse -= LBMR_TOPIC_OPT_UME_STORE_GROUP_SZ;
						break;
						case LBMR_TOPIC_OPT_UMQ_RCRIDX_TYPE:
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_UMQ_RCRIDX_TYPE ");
							 TS_printf(PRINT_VERBOSE, "\n ERROR!: not yet implemented:"); return 501;
						break;
						case LBMR_TOPIC_OPT_COST_TYPE:
							
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_COST_TYPE ");
							 opt_cost_ptr = (lbmr_topic_opt_cost_t *) ptr;
							 TS_printf(PRINT_DETAILS, "\n opt_cost_ptr->type = %i", opt_cost_ptr->type);
							 TS_printf(PRINT_DETAILS, "\n opt_cost_ptr->len = %i", opt_cost_ptr->len);
							 TS_printf(PRINT_DETAILS, "\n opt_cost_ptr->hop_count = %i", opt_cost_ptr->hop_count);
							 TS_printf(PRINT_DETAILS, "\n opt_cost_ptr->flags = %i", opt_cost_ptr->flags);
							 TS_printf(PRINT_DETAILS, "\n opt_cost_ptr->cost = %i", ntohl(opt_cost_ptr->cost));

							 if (ntohl(opt_cost_ptr->cost) == LBM_RESOLVE_TIR_OPTS_COST_INFINITE){
							   if( options.stats == 1 ){
								char evicted_source[LBM_MSG_MAX_SOURCE_LEN];
								cnt = ntohl(fromaddr->sin_addr.s_addr);
								snprintf(evicted_source,LBM_MSG_MAX_SOURCE_LEN-1, "EVICTED,%s[%i],%i.%i.%i.%i:%i",tname, msg_domain_id,
								((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),ntohs(fromaddr->sin_port));
								TS_stats_TMR( evicted_source, options_str, tv, TS_save_all_entries );
							    	TS_printf(PRINT_VERBOSE|PRINT_DETAILS, "\n Final Ads :%s", evicted_source);
							    }
							 }
							 ptr += LBMR_TOPIC_OPT_COST_SZ;
							 opt_bytes_to_parse -= LBMR_TOPIC_OPT_COST_SZ;
						break;
						case LBMR_TOPIC_OPT_OTID_TYPE:
							 TS_printf(PRINT_PCAP, "\n LBMR_TOPIC_OPT_OTID_TYPE ");
							 opt_oid_ptr = (lbmr_topic_opt_otid_t *) ptr;

							 TS_printf(PRINT_DETAILS, "\n opt_oid->type = %i", opt_oid_ptr->type);
							 TS_printf(PRINT_DETAILS, "\n opt_oid->len = %i", opt_oid_ptr->len);
							 TS_printf(PRINT_DETAILS, "\n opt_oid->flags = %x", ntohs(opt_oid_ptr->flags));

							/* OTID parsing */
							TS_printf (PRINT_OTIDS|PRINT_OTIDS, "\n opt_oid->otid = ");
							for (cnt =0; cnt < LBM_OTID_BLOCK_SZ; cnt++){
							 TS_printf(PRINT_OTIDS|PRINT_DRO, "%x", opt_oid_ptr->otid[cnt]);
							}
							if (opt_oid_ptr->len != LBMR_TOPIC_OPT_OTID_SZ ){
								TS_printf(PRINT_ERROR, "\n ERROR! LBMR_TOPIC_OPT_OTID_SZ = %i vs opt_oid->len = %i", LBMR_TOPIC_OPT_OTID_SZ, opt_oid_ptr->len );
								return 10000;
							}

							stats_otids++;
							otid_flag=1;
                                                        lbm_otid_to_transport_source(opt_oid_ptr->otid, &tinfo, sizeof(tinfo) );
        						source_size = LBM_MSG_MAX_SOURCE_LEN;
							/* Assuming OTID specified first, otherwise should cat source_name not print it */
                                                        cnt = lbm_transport_source_format(&tinfo, sizeof(tinfo), source_name, &source_size);
							if(cnt != 0 ){
								TS_printf(PRINT_ERROR, "\n ERROR! Could not parse OTID SRC: lbm_transport_source_format=%i", cnt );
								return 10001;
							}
                                                        TS_printf(PRINT_OTIDS, "\n[#%i] OTID_SOURCE: %s %s %s",stats_otids,  source_name, tname, options_str);
							snprintf(temp_buff,LBM_MSG_MAX_SOURCE_LEN-1, "Otid_topic:%s%s", tname, options_str);
							ptr += LBMR_TOPIC_OPT_OTID_SZ;
							opt_bytes_to_parse -= LBMR_TOPIC_OPT_OTID_SZ;
						break;
						case LBMR_TOPIC_OPT_ULB_TYPE:
						{
							lbmr_topic_opt_ulb_t opt_ulb_buf;
							stats_ulbs++;
							TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_ULB_TYPE ");
							opt_ulb_ptr = (lbmr_topic_opt_ulb_t *) ptr;

							opt_ulb_buf.type = opt_ulb_ptr->type;
							opt_ulb_buf.len = opt_ulb_ptr->len;
							opt_ulb_buf.flags = ntohs(opt_ulb_ptr->flags);
							opt_ulb_buf.src_tcp_port = ntohs(opt_ulb_ptr->src_tcp_port);
							opt_ulb_buf.src_id = ntohl(opt_ulb_ptr->src_id);
							opt_ulb_buf.src_ip_addr = ntohl(opt_ulb_ptr->src_ip_addr);
							opt_ulb_buf.queue_id = ntohl(opt_ulb_ptr->queue_id);
							
							
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n type (should be 11) = %i ", opt_ulb_buf.type);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n len = %i ", opt_ulb_buf.len);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n flags = %x ", opt_ulb_buf.flags);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n queue_id = %x ", opt_ulb_buf.queue_id);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n src_tcp_port : %i ", opt_ulb_buf.src_tcp_port);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n src_id = %x ", opt_ulb_buf.src_id);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n src_ip_addr = " );
							TS_pretty_print_addr(PRINT_DETAILS|PRINT_ULB, opt_ulb_buf.src_ip_addr);
							TS_printf(PRINT_DETAILS|PRINT_ULB, "\n reg_id = 0x");
							for (cnt =0; cnt < 8; cnt++){
							 	TS_printf(PRINT_DETAILS|PRINT_ULB, "%x", opt_ulb_ptr->reg_id[cnt]);
							 }

							 if( options.stats == 1 ){
								char ulb_buf[30];
								char ulb_src_name[300];
								sprintf(ulb_buf,"0x%x%x%x%x%x%x%x%x",opt_ulb_ptr->reg_id[0],opt_ulb_ptr->reg_id[1],
								opt_ulb_ptr->reg_id[2],opt_ulb_ptr->reg_id[3],opt_ulb_ptr->reg_id[4],opt_ulb_ptr->reg_id[5],
								opt_ulb_ptr->reg_id[6],opt_ulb_ptr->reg_id[7]);

								cnt = opt_ulb_buf.src_ip_addr;
								snprintf(ulb_src_name,LBM_MSG_MAX_SOURCE_LEN-1,
								"ULB[type:%i,flags:0x%x],queue_id:0x%x,src_id:0x%x,src_ip_addr:%i.%i.%i.%i:%i,regID:%s",
								 opt_ulb_buf.type,  opt_ulb_buf.flags, opt_ulb_buf.queue_id, opt_ulb_buf.src_id,
								 ((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),opt_ulb_buf.src_tcp_port,
								ulb_buf );

								cnt = ntohl(fromaddr->sin_addr.s_addr);
								snprintf(ulb_buf,LBM_MSG_MAX_SOURCE_LEN-1, ",LBMR:%i.%i.%i.%i:%i%s",
									((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
									ntohs(fromaddr->sin_port), options_str);

								TS_printf(PRINT_ULB,"\n%s%s", ulb_src_name,ulb_buf);
								TS_stats_ULB(ulb_src_name, ulb_buf, tv, TS_save_all_entries);
							 }

							ptr += LBMR_TOPIC_OPT_ULB_SZ;
							opt_bytes_to_parse -= LBMR_TOPIC_OPT_ULB_SZ;
						}
						break;
						case LBMR_TOPIC_OPT_EXFUNC_TYPE:
						{
							 lbmr_topic_opt_exfunc_t opt_exfunc_buf;

							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_EXFUNC_TYPE ");
							 opt_exfunc_ptr = (lbmr_topic_opt_exfunc_t *) ptr;
							 opt_exfunc_buf.flags = ntohs(opt_exfunc_ptr->flags);
							 opt_exfunc_buf.src_tcp_port = ntohs(opt_exfunc_ptr->src_tcp_port);
							 opt_exfunc_buf.src_ip_addr = htonl(opt_exfunc_ptr->src_ip_addr);
							 opt_exfunc_buf.functionality_flags = htonl(opt_exfunc_ptr->functionality_flags);

							 TS_printf(PRINT_DEBUG, "\n opt_exfunc_buf.type,  %i", opt_exfunc_buf.type);
							 TS_printf(PRINT_DEBUG, "\n opt_exfunc_buf.len,  %i", opt_exfunc_buf.len);
							 TS_printf(PRINT_DEBUG, "\n opt_exfunc_buf.flags,  %x: ", opt_exfunc_buf.flags);

							 if ((opt_exfunc_buf.flags & LBMR_TOPIC_OPT_EXFUNC_FLAG_IGNORE) == LBMR_TOPIC_OPT_EXFUNC_FLAG_IGNORE){
								TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_EXFUNC_FLAG_IGNORE");
							 }
							 TS_printf(PRINT_VERBOSE, "\n application request_tcp_port and IP:  %i ", opt_exfunc_buf.src_tcp_port);
							 TS_pretty_print_addr(PRINT_VERBOSE, opt_exfunc_buf.src_ip_addr);
							 TS_printf(PRINT_VERBOSE, "\n opt_exfunc_buf.functionality_flags %x: ", opt_exfunc_buf.functionality_flags);
							 if ((opt_exfunc_buf.functionality_flags & LBMR_TOPIC_OPT_EXFUNC_FFLAG_LJ )  == LBMR_TOPIC_OPT_EXFUNC_FFLAG_LJ){
							 	TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_EXFUNC_FFLAG_LJ ");
							 }
							 if ((opt_exfunc_buf.functionality_flags & LBMR_TOPIC_OPT_EXFUNC_FFLAG_UME )  == LBMR_TOPIC_OPT_EXFUNC_FFLAG_UME){
							 	TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_EXFUNC_FFLAG_UME ");
								stats_umps++;
								ex_func_flag = 1;
							   	if( options.stats == 1 ){
									cnt = opt_exfunc_buf.src_ip_addr;
									snprintf(ex_func_str,LBM_MSG_MAX_SOURCE_LEN-1, ",UMP_SRC_lj[%i.%i.%i:%i:%i]",
									((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),opt_exfunc_buf.src_tcp_port);
							   	}
							 }
							 if ((opt_exfunc_buf.functionality_flags & LBMR_TOPIC_OPT_EXFUNC_FFLAG_UMQ )  == LBMR_TOPIC_OPT_EXFUNC_FFLAG_UMQ){
							 	TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_EXFUNC_FFLAG_UMQ ");
							   	if( options.stats == 1 ){
									cnt = opt_exfunc_buf.src_ip_addr;
									snprintf(ex_func_str,LBM_MSG_MAX_SOURCE_LEN-1, ",UMQ_SRC_lj[%i.%i.%i:%i:%i]",
									((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),opt_exfunc_buf.src_tcp_port);
							   	}
							 }
							 if ((opt_exfunc_buf.functionality_flags & LBMR_TOPIC_OPT_EXFUNC_FFLAG_ULB )  == LBMR_TOPIC_OPT_EXFUNC_FFLAG_ULB){
							 	TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_EXFUNC_FFLAG_ULB ");
							   	if( options.stats == 1 ){
									cnt = opt_exfunc_buf.src_ip_addr;
									snprintf(ex_func_str,LBM_MSG_MAX_SOURCE_LEN-1, ",ULB_SRC_lj[%i.%i.%i:%i:%i]",
									((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),opt_exfunc_buf.src_tcp_port);
							   	}
							 }
							 ptr += LBMR_TOPIC_OPT_EXFUNC_SZ;
							 opt_bytes_to_parse -= LBMR_TOPIC_OPT_EXFUNC_SZ;
						}
						break;
						case LBMR_TOPIC_OPT_CTXINSTS_TYPE:
							 TS_printf(PRINT_UMP, "\n STORE LBMR_TOPIC_OPT_CTXINSTS_TYPE ");
							/* Fall through */
						case LBMR_TOPIC_OPT_CTXINSTQ_TYPE:
							 TS_printf(PRINT_ULB, "\n QUEUE LBMR_TOPIC_OPT_CTXINSTQ_TYPE ");
							/* Fall through */
						case LBMR_TOPIC_OPT_CTXINST_TYPE:
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_CTXINST_TYPE ");
							 opt_ctxinst_ptr = (lbmr_topic_opt_ctxinst_t *) ptr;
							 if ((opt_ctxinst_ptr->flags & LBMR_TOPIC_OPT_CTXINST_TYPE) == LBMR_TOPIC_OPT_CTXINST_TYPE){
								TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_CTXINST_TYPE ");
							 }
							 if ((opt_ctxinst_ptr->flags & LBMR_TOPIC_OPT_CTXINST_FLAG_IGNORE) == LBMR_TOPIC_OPT_CTXINST_FLAG_IGNORE){
								TS_printf(PRINT_VERBOSE, " LBMR_TOPIC_OPT_CTXINST_FLAG_IGNORE ");
							 }
							
							 TS_printf(PRINT_DEBUG, "\n opt_ctxinst_ptr->type,  %i", opt_ctxinst_ptr->type);
							 TS_printf(PRINT_DEBUG, "\n opt_ctxinst_ptr->len,  %i", opt_ctxinst_ptr->len);
							 TS_printf(PRINT_DEBUG, "\n opt_ctxinst_ptr->flags,  %x: ", opt_ctxinst_ptr->flags);
							 TS_printf(PRINT_DEBUG, "\n opt_ctxinst_ptr->idx,  %x: ", opt_ctxinst_ptr->idx);

							 for (cnt =0; cnt < LBM_CONTEXT_INSTANCE_BLOCK_SZ; cnt++){
							 	TS_printf(PRINT_VERBOSE|PRINT_UMP, "%x", opt_ctxinst_ptr->ctxinst[cnt]);
							 }
							 ptr += LBMR_TOPIC_OPT_CTXINST_SZ;
							 opt_bytes_to_parse -= LBMR_TOPIC_OPT_CTXINSTQ_SZ;
						break;
						case LBMR_TOPIC_OPT_DOMAIN_ID_TYPE:
						{
							 uint32_t domain_id_val;
							
							 TS_printf(PRINT_VERBOSE, "\n LBMR_TOPIC_OPT_DOMAIN_ID_TYPE ");
							 topic_opt_domain_id_ptr = (lbmr_topic_opt_domain_id_t *) ptr;

							 domain_id_val = ntohl(topic_opt_domain_id_ptr->domain_id);

							 TS_printf(PRINT_DEBUG, "\n topic_opt_domain_id_ptr->type,  %i", topic_opt_domain_id_ptr->type);
							 TS_printf(PRINT_DEBUG, "\n topic_opt_domain_id_ptr->len,  %i", topic_opt_domain_id_ptr->len);
							 TS_printf(PRINT_DEBUG, "\n topic_opt_domain_id_ptr->flags,  %x: ", topic_opt_domain_id_ptr->flags);

							 TS_printf(PRINT_DRO, "\n topic_opt_domain_id_buf.domain_id,  %i: ", domain_id_val);
							 stats_dro_src++;
							 dro_src_flag=1;
							 if( options.stats == 1 ){
								char domain_str[50];	
								snprintf(domain_str, 50, ",RDomainID:%i",domain_id_val);
								/* Comes after the OTID entry as far as I can tell, so appending source_name */
								if( strlen(source_name) < (LBM_MSG_MAX_SOURCE_LEN - 50)){
									strncat(source_name,domain_str,LBM_MSG_MAX_SOURCE_LEN);
								} else {
									TS_printf(PRINT_ERROR,"Could not append domain_str to source_name");
									return 665;
								}
							 }
							 ptr += LBMR_TOPIC_OPT_DOMAIN_ID_SZ;
							 opt_bytes_to_parse -= LBMR_TOPIC_OPT_DOMAIN_ID_SZ;
						}
						break;
						/* case LBMR_TOPIC_OPT_FLAG_IGNORE: */
						default:
							TS_printf(PRINT_ERROR, "\n ERROR! default t_opthdr_ptr->type: case: %i", t_opthdr_ptr->type);
							return 666;
						break;
					}
					TS_printf(PRINT_DEBUG, "\n ***************************** opt_bytes_to_parse = %i", opt_bytes_to_parse);
				  } /* end while ( total_len > 0 )*/

				}/* End if TIR OPTIONS */

				/* Storing the OTID stat. here because the remote domain option may have been added */
				if( options.stats == 1 ){
					if(ex_func_flag == 1){
						strncat(source_name,ex_func_str,LBM_MSG_MAX_SOURCE_LEN-1 );
					}
					if((otid_flag==1) || (dro_src_flag==1)) {
						TS_stats_OTID( source_name, temp_buff, tv, TS_save_all_entries );
					}
				}
				otid_flag=0; dro_src_flag=0; ex_func_flag=0;

				{ /* Parse the TIR transport */
				  char transp_str[100];
				  uint32_t topic_idx = ntohl(tir_ptr->index);
				  switch (tir_ptr->transport & LBMR_TIR_TRANSPORT){
					case LBMR_TRANSPORT_TCP:
						if (tir_ptr->tlen == LBMR_TIR_TCP_SZ ) {
							lbmr_tir_tcp_t tir_tcp_buf;
							TS_printf(PRINT_VERBOSE, "\n tir_ptr->tlen == LBMR_TIR_TCP_SZ %i ", LBMR_TIR_TCP_SZ);
							tir_tcp_ptr = (lbmr_tir_tcp_t *) ptr;
							tir_tcp_buf.ip = ntohl(tir_tcp_ptr->ip);
							tir_tcp_buf.port = ntohs(tir_tcp_ptr->port);
							TS_printf(PRINT_VERBOSE, "\n tir_tcp_ptr->ip = %x ", tir_tcp_buf.ip);
							TS_printf(PRINT_VERBOSE, "\n tir_tcp_ptr->port = %i ", tir_tcp_buf.port);
							cnt = tir_tcp_buf.ip;
							snprintf(transp_str,50-1, "TCP:%i.%i.%i.%i:%u[%u]",
							((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
							tir_tcp_buf.port,topic_idx);
							TS_printf(PRINT_DEBUG,"LBMR_TIR_TCP_SZ:%s",transp_str);
						} else if ( tir_ptr->tlen == LBMR_TIR_TCP_WITH_SID_SZ ){
							lbmr_tir_tcp_with_sid_t  tir_tcp_with_sid_buf ;
							tir_tcp_with_sid_ptr = (lbmr_tir_tcp_with_sid_t *) ptr;
							tir_tcp_with_sid_buf.ip = ntohl(tir_tcp_with_sid_ptr->ip);
							tir_tcp_with_sid_buf.session_id =  ntohl(tir_tcp_with_sid_ptr->session_id);
							tir_tcp_with_sid_buf.port = ntohs(tir_tcp_with_sid_ptr->port);
							TS_printf(PRINT_VERBOSE, "\n tir_ptr->tlen == LBMR_TIR_TCP_WITH_SID_SZ %i ", LBMR_TIR_TCP_WITH_SID_SZ);
							TS_printf(PRINT_VERBOSE, "\n tir_tcp_with_sidbuf.ip = %x ", tir_tcp_with_sid_buf.ip);
							TS_printf(PRINT_VERBOSE, "\n tir_tcp_with_sidbuf.session_id = 0x%x ", tir_tcp_with_sid_buf.session_id);
							TS_printf(PRINT_VERBOSE, "\n tir_tcp_with_sidbuf.port = %i ", tir_tcp_with_sid_buf.port);
							TS_printf(PRINT_DEBUG,"%s",transp_str);
							snprintf(transp_str,50-1, "TCP:%u:%x[%u]",tir_tcp_with_sid_buf.port,tir_tcp_with_sid_buf.session_id,topic_idx);
							cnt = tir_tcp_with_sid_buf.ip;
							snprintf(transp_str,50-1, "TCP:%i.%i.%i.%i:%u:%x[%u]",
								((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
								 tir_tcp_with_sid_buf.port,tir_tcp_with_sid_buf.session_id,topic_idx);
							TS_printf(PRINT_DEBUG,"LBMR_TIR_TCP_WITH_SID_SZ:%s",transp_str);
						} else {
							TS_printf(PRINT_ERROR, "\n ERROR! Invalid Transport Len %i LBMR_TRANSPORT_TCP ", tir_ptr->tlen );
							return 1000;
						}

					break;
					case LBMR_TRANSPORT_LBTRU:
						if (tir_ptr->tlen == LBMR_TIR_LBTRU_SZ ) {
							lbmr_tir_lbtru_t tir_lbtru_buf;
							TS_printf(PRINT_VERBOSE, "\n tir_ptr->tlen == LBMR_TIR_LBTRU_SZ %i ", LBMR_TIR_LBTRU_SZ);
							tir_lbtru_ptr = (lbmr_tir_lbtru_t *) ptr;
							tir_lbtru_buf.ip = ntohl(tir_lbtru_ptr->ip);
							tir_lbtru_buf.port = ntohs(tir_lbtru_ptr->port);
							TS_printf(PRINT_VERBOSE, "\n tir_lbtru_buf.ip = %x ", tir_lbtru_buf.ip);
							TS_printf(PRINT_VERBOSE, "\n tir_lbtru_buf.port = %i ", tir_lbtru_buf.port);
							cnt = tir_lbtru_buf.ip;
							snprintf(transp_str,50-1, "LBT-RU:%i.%i.%i.%i:%u[%u]",
							((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
							tir_lbtru_buf.port,topic_idx);
							TS_printf(PRINT_DEBUG,"LBMR_TIR_LBTRU_SZ:%s",transp_str);
						} else if ( tir_ptr->tlen == LBMR_TIR_LBTRU_WITH_SID_SZ ){
							lbmr_tir_lbtru_with_sid_t tir_lbtru_with_sid_buf ;
							tir_lbtru_with_sid_ptr = (lbmr_tir_lbtru_with_sid_t *) ptr;
							tir_lbtru_with_sid_buf.ip = ntohl(tir_lbtru_with_sid_ptr->ip);
							tir_lbtru_with_sid_buf.session_id =  ntohl(tir_lbtru_with_sid_ptr->session_id);
							tir_lbtru_with_sid_buf.port = ntohs(tir_lbtru_with_sid_ptr->port);
							TS_printf(PRINT_VERBOSE, "\n tir_buf.tlen == LBMR_TIR_LBTRU_WITH_SID_SZ %i ", LBMR_TIR_LBTRU_WITH_SID_SZ);
							TS_printf(PRINT_VERBOSE, "\n tir_lbtru_with_sid_buf.ip = %x ", tir_lbtru_with_sid_buf.ip);
							TS_printf(PRINT_VERBOSE, "\n tir_lbtru_with_sid_buf.session_id = 0x%x ", tir_lbtru_with_sid_buf.session_id);
							TS_printf(PRINT_VERBOSE, "\n tir_lbtru_with_sid_buf.port = %i ", tir_lbtru_with_sid_buf.port);
							cnt = tir_lbtru_with_sid_buf.ip;
							snprintf(transp_str,50-1, "LBT-RU:%i.%i.%i.%i:%u:%x[%u]",
							((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
							tir_lbtru_with_sid_buf.port,tir_lbtru_with_sid_buf.session_id,topic_idx);
							TS_printf(PRINT_DEBUG,"LBMR_TIR_LBTRU_WITH_SID_SZ:%s",transp_str);

						} else {
							TS_printf(PRINT_ERROR, "\n ERROR! Invalid Transport Len %i LBMR_TRANSPORT_LBTRU ", tir_ptr->tlen );
							return 1001;
						}
					break;
					case LBMR_TRANSPORT_TCP6:
						TS_printf(PRINT_ERROR, "\n ERROR! LBMR_TRANSPORT_TCP6 invalid! Not implemented ");
						     	return 1002;
					break;
					case LBMR_TRANSPORT_LBTSMX:
		                             if (tir_ptr->tlen == LBMR_TIR_LBTSMX_SZ) {
						lbmr_tir_lbtsmx_t tir_lbtsmx_buf;
						tir_lbtsmx_ptr = ( lbmr_tir_lbtsmx_t *) ptr;
						tir_lbtsmx_buf.host_id = ntohl(tir_lbtsmx_ptr->host_id);
						tir_lbtsmx_buf.session_id = ntohl(tir_lbtsmx_ptr->session_id);
						tir_lbtsmx_buf.xport_id = ntohs(tir_lbtsmx_ptr->xport_id);
						snprintf(transp_str,100-1, "LBT-SMX:%x:%u[%u],hid:0x%x(%i.%i.%i.%i)",
								tir_lbtsmx_buf.session_id,
								tir_lbtsmx_buf.xport_id,
								topic_idx,
								tir_lbtsmx_buf.host_id,
								/* Should double check this */
								((tir_lbtsmx_buf.host_id >> 24) & 0xFF),
								((tir_lbtsmx_buf.host_id >> 16) & 0xFF),
								((tir_lbtsmx_buf.host_id >> 8) & 0xFF),
								((tir_lbtsmx_buf.host_id) & 0xFF)
								);
						TS_printf(PRINT_DEBUG, "\nLBMR_TRANSPORT_LBTSMX:%s",transp_str);
					    } else {
						TS_printf(PRINT_ERROR, "\n ERROR! invalid Transport Len,  %i  LBMR_TRANSPORT_LBTSMX ",  tir_ptr->tlen);
							return 1003;
					    }
					break;
					case LBMR_TRANSPORT_LBTRM:
		                             if (tir_ptr->tlen == LBMR_TIR_LBTRM_SZ) {
						lbmr_tir_lbtrm_t tir_lbtrm_buf;
						tir_lbtrm_ptr = ( lbmr_tir_lbtrm_t *) ptr;
						tir_lbtrm_buf.src_addr = ntohl(tir_lbtrm_ptr->src_addr);
						tir_lbtrm_buf.mcast_addr =  ntohl(tir_lbtrm_ptr->mcast_addr);
						tir_lbtrm_buf.session_id = ntohl(tir_lbtrm_ptr->session_id);
						tir_lbtrm_buf.udp_dest_port = ntohs(tir_lbtrm_ptr->udp_dest_port);
						tir_lbtrm_buf.src_ucast_port = ntohs(tir_lbtrm_ptr->src_ucast_port);
						snprintf(transp_str,100-1,"LBT-RM:%i.%i.%i.%i:%u:%x:%i.%i.%i.%i:%u[%u]",
                                                                		((tir_lbtrm_buf.src_addr >> 24) & 0xFF),
                                                                		((tir_lbtrm_buf.src_addr >> 16) & 0xFF),
                                                                		((tir_lbtrm_buf.src_addr >> 8) & 0xFF),
                                                                		(tir_lbtrm_buf.src_addr & 0xFF),
                                                                         (tir_lbtrm_buf.src_ucast_port),
                                                                         (tir_lbtrm_buf.session_id),
                                                                		((tir_lbtrm_buf.mcast_addr >> 24) & 0xFF),
                                                                		((tir_lbtrm_buf.mcast_addr >> 16) & 0xFF),
                                                                		((tir_lbtrm_buf.mcast_addr >> 8) & 0xFF),
                                                                		(tir_lbtrm_buf.mcast_addr & 0xFF),
                                                                         (tir_lbtrm_buf.udp_dest_port),
                                                                         topic_idx);
						TS_printf(PRINT_DEBUG, "\nLBMR_TRANSPORT_LBTRM:%s",transp_str);
						} else {
							TS_printf(PRINT_VERBOSE, "\n ERROR! Invalid Transport Len %i LBMR_TRANSPORT_LBTRM ", tir_ptr->tlen );
							return 1004;
						}
					break;
					case LBMR_TRANSPORT_LBTRDMA:
		                             if (tir_ptr->tlen == LBMR_TIR_LBTRDMA_SZ) {
						lbmr_tir_lbtrdma_t tir_lbtrdma_buf;
						tir_lbtrdma_ptr = ( lbmr_tir_lbtrdma_t *) ptr;
						tir_lbtrdma_buf.ip = ntohl(tir_lbtrdma_ptr->ip);
						tir_lbtrdma_buf.session_id = ntohl(tir_lbtrdma_ptr->session_id);
						tir_lbtrdma_buf.port = ntohs(tir_lbtrdma_ptr->port);
						snprintf(transp_str,100-1,"LBT-RDMA:%i.%i.%i.%i:%u:%x[%u]",
                                                               ((tir_lbtrdma_buf.ip >> 24) & 0xFF),
                                                               ((tir_lbtrdma_buf.ip >> 16) & 0xFF),
                                                               ((tir_lbtrdma_buf.ip >> 8) & 0xFF),
                                                               (tir_lbtrdma_buf.ip & 0xFF),
								tir_lbtrdma_buf.port,
								tir_lbtrdma_buf.session_id,
								topic_idx );
						TS_printf(PRINT_DEBUG, "\nLBTRDMA:%s",transp_str);
						
					    } else {
						TS_printf(PRINT_VERBOSE, "\n ERROR! invalid Transport Len %i LBMR_TRANSPORT_LBTRDMA ", tir_ptr->tlen);
							return 1005;
					    }
					break;
					case LBMR_TRANSPORT_LBTIPC:
		                             if (tir_ptr->tlen == LBMR_TIR_LBTIPC_SZ) {

						lbmr_tir_lbtipc_t tir_lbtipc_buf;
						tir_lbtipc_ptr = ( lbmr_tir_lbtipc_t *) ptr;
						tir_lbtipc_buf.host_id = ntohl(tir_lbtipc_ptr->host_id);
						tir_lbtipc_buf.session_id = ntohl(tir_lbtipc_ptr->session_id);
						tir_lbtipc_buf.xport_id = ntohs(tir_lbtipc_ptr->xport_id);
						snprintf(transp_str,100-1, "LBT-IPC:%x:%u[%u],hid:0x%x(%i.%i.%i.%i)",
								tir_lbtipc_buf.session_id,
								tir_lbtipc_buf.xport_id,
								topic_idx,
								tir_lbtipc_buf.host_id ,
								((tir_lbtipc_buf.host_id >> 24) & 0xFF),
								((tir_lbtipc_buf.host_id >> 16) & 0xFF),
								((tir_lbtipc_buf.host_id >> 8) & 0xFF),
								((tir_lbtipc_buf.host_id) & 0xFF)
								  );
						TS_printf(PRINT_DEBUG, "\nLBMR_TRANSPORT_LBTIPC:%s",transp_str);
					    } else {
						TS_printf(PRINT_VERBOSE, "\n ERROR! invalid Transport Len, %i LBMR_TRANSPORT_LBTIPC ", tir_ptr->tlen);
							return 1006;
					    }
					break;
					case LBMR_TRANSPORT_PGM: /* fall through */
					default:
						TS_printf(PRINT_VERBOSE, "\n Not yet implemented ");
							return 1007;
				  }/* End switch on transport */
				  /* Point to the next transport */
				  ptr += tir_ptr->tlen;
				}  /* End parsing the TIR transport */
					
			stats_tirs++;
			}/* End for */
			//TS_printf(PRINT_VERBOSE, "\n bytes_to_parse = %i", bytes_to_parse);
		} /* End parse TIRs */
		
	  break;
	  case LBMR_HDR_TYPE_TOPIC_MGMT:
	  {
		lbmr_tmb_t tmb_buf;
		TS_printf(PRINT_VERBOSE|PRINT_TMR, "\n LBMR_HDR_TYPE_TOPIC_MGMT: ");

		ptr = pktData + LBMR_HDR_SZ;
		tmb_ptr = (lbmr_tmb_t *) ptr;
		tmb_buf.len = ntohs(tmb_ptr->len);
		tmb_buf.tmrs = ntohs(tmb_ptr->tmrs);
		if ( tmb_buf.len < LBMR_TMB_SZ ) {
			TS_printf(PRINT_WARNING|PRINT_TMR, "\n WARNING! tmb_buf.len(%i) < LBMR_TMR_SZ (%i)", tmb_buf.len , LBMR_TMB_SZ);
		}
		ptr += LBMR_TMB_SZ;
		tmr_ptr = (lbmr_tmr_t *)ptr;
		if ( tmb_buf.tmrs > 1000 ) { /* thats a lot */
			TS_printf(PRINT_ERROR|PRINT_TMR, "\n ERROR! tmb_buf.tmrs(%i) > 1000", tmb_buf.tmrs);
			return 1015;
		}
		TS_printf(PRINT_VERBOSE|PRINT_TMR, "\n LBMR TMB Length[%i] for tmrs[%i]", tmb_buf.len, tmb_buf.tmrs);
		

		lenparsed = 0;
		for ( ctr = 0 ;(ctr < tmb_buf.tmrs) && (lenparsed < tmb_buf.len); ctr++){
			uint16_t tmr_ptr_len;
	 		TSdump(PRINT_DEBUG, (char * )tmr_ptr, 10);	
			stats_tmrs++;
			
			source_name[LBM_MAX_TOPIC_NAME_LEN-1] = 0;
			strncpy(source_name, (char *)(tmr_ptr + LBMR_TMR_SZ), LBM_MAX_TOPIC_NAME_LEN);
			TS_printf(PRINT_TMR, "\n [#%i]LBMR TMR Len[%i]type[0x%02hhx]flags[0x%02hhx][%s][%u]",stats_tmrs,
				                tmr_ptr->len, tmr_ptr->type, tmr_ptr->flags,
                                                        source_name, strlen(source_name));
			if (source_name[LBM_MAX_TOPIC_NAME_LEN-1] != 0) {
				TS_printf(PRINT_ERROR, "\n ERROR! TMR topic name too large ");
				return 1010;
			}
			tmr_ptr_len = ntohs(tmr_ptr->len);
			TS_printf(PRINT_VERBOSE, "\n\t Topic Name: [ %s ]\t", source_name);
			switch( tmr_ptr->type ) {
			 case LBMR_TMR_LEAVE_TOPIC:
				strcat( source_name, TS_tmr_leave_str );
				TS_printf(PRINT_VERBOSE|PRINT_TMR, "%s", TS_tmr_leave_str );
			 break;
			 case LBMR_TMR_TOPIC_USE:
				strcat( source_name, TS_tmr_topic_use);
				TS_printf(PRINT_VERBOSE|PRINT_TMR, "%s", TS_tmr_topic_use);
			 break;
			 default:
				TS_printf(PRINT_ERROR|PRINT_TMR, ": ERROR! Invalid TMR type ");
				return 1011;
			 break;
			}
			
			if ((tmr_ptr->flags & LBMR_TMR_FLAG_RESPONSE) == LBMR_TMR_FLAG_RESPONSE ){
				strcat( source_name, TS_tmr_response_str);
				TS_printf(PRINT_VERBOSE|PRINT_TMR, "%s", TS_tmr_response_str);
			}
			if ((tmr_ptr->flags & LBMR_TMR_FLAG_WILDCARD_PCRE) == LBMR_TMR_FLAG_WILDCARD_PCRE){
				strcat( source_name, TS_pcre_str);
				TS_printf(PRINT_VERBOSE|PRINT_TMR, "%s", TS_pcre_str);
			}
			if ((tmr_ptr->flags & LBMR_TMR_FLAG_WILDCARD_REGEX) == LBMR_TMR_FLAG_WILDCARD_REGEX){
				strcat( source_name, TS_regex_str);
				TS_printf(PRINT_VERBOSE|PRINT_TMR, "%s", TS_regex_str);
			}

			if( options.stats == 1 ){
				TS_stats_TMR(source_name, options_str, tv, TS_save_all_entries);
			}

			lenparsed += tmr_ptr_len;
			ptr += tmr_ptr_len;
			tmr_ptr = (lbmr_tmr_t *) ptr;
		}/* End for */
			
	  }
	  break;
	  case LBMR_HDR_TYPE_QUEUE_RES:
		TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_TYPE_QUEUE_RES");
		TS_printf(PRINT_ERROR, "\n LBMR_HDR_TYPE_QUEUE_RES: not yet implemented ");
		return 1100;
	  break;
	  case LBMR_HDR_TYPE_EXT:
	     	 TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_TYPE_EXT");
		 ext_type_ptr = (lbmr_hdr_ext_type_t  *) pktData;

		switch (ext_type_ptr->ext_type ) {
	  	  case LBMR_HDR_EXT_TYPE_UME_PROXY_SRC_ELECT:
			TS_printf(PRINT_UMP, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_UME_PROXY_SRC_ELECT");
			TS_printf(PRINT_WARNING, "  :DEPRECATED ");
	  	  break;
	  	  case LBMR_HDR_EXT_TYPE_UMQ_QUEUE_MGMT:
			TS_printf(PRINT_ULB, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_UMQ_QUEUE_MGMT");
			TS_printf(PRINT_WARNING, "  :DEPRECATED ");
	  	  break;
	 	  case LBMR_HDR_EXT_TYPE_CONTEXT_INFO:
			TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_CONTEXT_INFO");
			TS_printf(PRINT_WARNING, "  :DEPRECATED ");
	  	  break;
	  	  case LBMR_HDR_EXT_TYPE_TOPIC_RES_REQUEST:
			TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_TOPIC_RES_REQUEST\n");
			topic_res_request_ptr = (lbmr_topic_res_request_t *) pktData;
			topic_res_request_ptr->flags = ntohs(topic_res_request_ptr->flags);
			// Check here if header size == LBMR_TOPIC_RES_REQUEST_SZ ?
			if( (topic_res_request_ptr->flags & LBM_TOPIC_RES_REQUEST_ADVERTISEMENT) != 0) {
				TS_printf(PRINT_VERBOSE, "  LBM_TOPIC_RES_REQUEST_ADVERTISEMENT");
			}
			if( (topic_res_request_ptr->flags & LBM_TOPIC_RES_REQUEST_QUERY) != 0) {
				TS_printf(PRINT_VERBOSE, "  LBM_TOPIC_RES_REQUEST_QUERY");
			}
			if( (topic_res_request_ptr->flags & LBM_TOPIC_RES_REQUEST_WILDCARD_QUERY) != 0) {
				TS_printf(PRINT_VERBOSE, "  LBM_TOPIC_RES_REQUEST_WILDCARD_QUERY");
			}
			if( (topic_res_request_ptr->flags & LBM_TOPIC_RES_REQUEST_CONTEXT_QUERY  ) != 0) {
				TS_printf(PRINT_VERBOSE, "  LBM_TOPIC_RES_REQUEST_CONTEXT_QUERY  ");
			}
			if( (topic_res_request_ptr->flags & LBM_TOPIC_RES_REQUEST_GW_REMOTE_INTEREST  ) != 0) {
				TS_printf(PRINT_VERBOSE, "  LBM_TOPIC_RES_REQUEST_GW_REMOTE_INTEREST ");
			}
	  	  break;
	  	  case LBMR_HDR_EXT_TYPE_TNWG_MSG:
			{
			  lbmr_tnwg_t lbmr_tnwg_t_buf;
			  lbmr_tnwg_t *lbmr_tnwg_t_ptr;
			  TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_TNWG_MSG");
			
			  lbmr_tnwg_t_ptr = (lbmr_tnwg_t *) pktData;
			  lbmr_tnwg_t_buf.type =  ntohs(lbmr_tnwg_t_ptr->type);
			  lbmr_tnwg_t_buf.len =  ntohs(lbmr_tnwg_t_ptr->len);
                          TS_printf(PRINT_DEBUG,"\n LBMR_TNWG_TYPE_INTEREST: lbmr_tnwg_t_buf.type=%i, len=%i", lbmr_tnwg_t_buf.type, lbmr_tnwg_t_buf.len);
			
			  ptr = pktData + LBMR_TNWG_BASE_SZ;
        		  switch (lbmr_tnwg_t_buf.type) {
                		case LBMR_TNWG_TYPE_INTEREST:
					{
			  		  lbmr_tnwg_interest_t *lbmr_tnwg_interest_t_ptr;
			  		  lbmr_tnwg_interest_t lbmr_tnwg_interest_t_buf;

					  lbmr_tnwg_interest_t_ptr = (lbmr_tnwg_interest_t *) ptr;
					  lbmr_tnwg_interest_t_buf.count = ntohs(lbmr_tnwg_interest_t_ptr->count);
					  lbmr_tnwg_interest_t_buf.len = ntohs(lbmr_tnwg_interest_t_ptr->len);
                        		  TS_printf(PRINT_DETAILS,"\n LBMR_TNWG_TYPE_INTEREST: count=%i, len=%i", lbmr_tnwg_interest_t_buf.count, lbmr_tnwg_interest_t_buf.len);
					  lenparsed = 0;
					  ptr += LBMR_TNWG_INTEREST_SZ;
					  for(ctr=0; (ctr < lbmr_tnwg_interest_t_buf.count) && (lenparsed < lbmr_tnwg_interest_t_buf.len); ctr++){
					  	lbmr_tnwg_interest_rec_t * lbmr_tnwg_interest_rec_t_ptr;
					  	lbmr_tnwg_interest_rec_t lbmr_tnwg_interest_rec_t_buf;
						char *symbol;
						char int_str[TS_MAX_STRING_LENGTH];
						char *flag_str;

						lbmr_tnwg_interest_rec_t_ptr = (lbmr_tnwg_interest_rec_t *) ptr;
						symbol = (char *)(ptr+LBMR_TNWG_INTEREST_REC_BASE_SZ);
						lbmr_tnwg_interest_rec_t_buf.len = ntohs(lbmr_tnwg_interest_rec_t_ptr->len);
						lbmr_tnwg_interest_rec_t_buf.domain_id = ntohl(lbmr_tnwg_interest_rec_t_ptr->domain_id);
						lbmr_tnwg_interest_rec_t_buf.flags = lbmr_tnwg_interest_rec_t_ptr->flags;
						lbmr_tnwg_interest_rec_t_buf.pattype = lbmr_tnwg_interest_rec_t_ptr->pattype;
						TS_printf(PRINT_DRO,"\nlbmr_tnwg_interest_rec_t len:%i,flags:0x%x,pattype:%i,domain_id:%i,symbol:%s",
							lbmr_tnwg_interest_rec_t_buf.len, lbmr_tnwg_interest_rec_t_buf.flags,
							lbmr_tnwg_interest_rec_t_buf.pattype, lbmr_tnwg_interest_rec_t_buf.domain_id,symbol);

						cnt = ntohl(fromaddr->sin_addr.s_addr);
						snprintf(int_str,TS_MAX_STRING_LENGTH,"DRO_INTEREST{flags:%i,pattype:%i},domain_id:%i,symbol:%s,LBMR:%i.%i.%i.%i:%i",
							lbmr_tnwg_interest_rec_t_buf.flags, lbmr_tnwg_interest_rec_t_buf.pattype,
							lbmr_tnwg_interest_rec_t_buf.domain_id,symbol,
							((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),ntohs(fromaddr->sin_port));

						switch(lbmr_tnwg_interest_rec_t_buf.flags){
						 case LBMR_TNWG_INTEREST_REC_PATTERN_FLAG:
							flag_str = ",REC_PATTERN";
						 break;
						 case LBMR_TNWG_INTEREST_REC_CANCEL_FLAG:
							flag_str = ",REC_CANCEL";
						 break;
						 case LBMR_TNWG_INTEREST_REC_REFRESH_FLAG:
							flag_str = ",REC_REFRESH";
						 break;
						 default:
							flag_str = " ";
						 break;
						}
						TS_stats_DRO(int_str, flag_str, tv, TS_save_all_entries);
						TS_printf(PRINT_DEBUG,"\n int_str: %s", int_str);
					 	ptr += lbmr_tnwg_interest_rec_t_buf.len;
						lenparsed += lbmr_tnwg_interest_rec_t_buf.len;
					  }
					}
                        	break;
                		case LBMR_TNWG_TYPE_CTXINFO:
                        		printf("\n LBMR_TNWG_TYPE_CTXINFO: Not yet implemented XXXXXXXXXXXXXXXXXXXXXXXX ");
					return 1300;
                        	break;
                  		case LBMR_TNWG_TYPE_TRREQ:
                        		printf("\n LBMR_TNWG_TYPE_TRREQ: Not yet implemented XXXXXXXXXXXXXXXXXXXXXXXX ");
					return 1301;
	  	  		break;
				default:
					TS_printf(PRINT_ERROR,"Invalid lbmr_tnwg_t_ptr.type [%i]", lbmr_tnwg_t_ptr->type);
					return 1302;
			  }
			}
		  break;
	  	  case LBMR_HDR_EXT_TYPE_REMOTE_DOMAIN_ROUTE:
		    {
			uint16_t num_domains = 0;

			TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_REMOTE_DOMAIN_ROUTE");
			route_hdr_ptr = (lbmr_remote_domain_route_hdr_t *)pktData;
			TS_printf(PRINT_DEBUG, "\n Expect byte 0x1f for version(1) Options (1) and Extended (111)");
			TS_printf(PRINT_DEBUG, "\n ******* route_hdr_ptr.ver_type = %x",  route_hdr_ptr->ver_type);
			TS_printf(PRINT_DEBUG, "\n ******* route_hdr_ptr.ext_type = %x", route_hdr_ptr->ext_type);
			TS_printf(PRINT_DRO, "\n ******* route_hdr_ptr.ip = ");
				TS_pretty_print_addr(PRINT_DRO,  ntohl(route_hdr_ptr->ip));
			TS_printf(PRINT_DRO, "\n ******* route_hdr_ptr.port = %i", ntohs(route_hdr_ptr->port));
			TS_printf(PRINT_DETAILS, "\n ******* route_hdr_ptr.reserved s= %x", ntohs(route_hdr_ptr->reserved));
			TS_printf(PRINT_DEBUG, "\n ******* route_hdr_ptr.length l= %i", ntohl(route_hdr_ptr->length));
			num_domains = ntohs(route_hdr_ptr->num_domains);
			TS_printf(PRINT_DRO, "\n ******* route_hdr_ptr.num_domains = %i", num_domains);
			TS_printf(PRINT_DEBUG, "\n LBMR_REMOTE_DOMAIN_ROUTE_HDR_SZ = %i ", LBMR_REMOTE_DOMAIN_ROUTE_HDR_SZ);
			
			cnt = ntohl(fromaddr->sin_addr.s_addr);
			cnt2 = ntohl(route_hdr_ptr->ip);
			if( options.stats == 1 ){
				snprintf(source_name,LBM_MSG_MAX_SOURCE_LEN-1, "LBMR:%i.%i.%i.%i:%i,R_DOMAIN_ROUTE:%i.%i.%i.%i:%i,to %i domain(s):",
					((cnt >> 24) & 0xFF),  ((cnt >> 16) & 0xFF),  ((cnt >> 8) & 0xFF),  ((cnt) & 0xFF),  ntohs(fromaddr->sin_port),
					((cnt2 >> 24) & 0xFF), ((cnt2 >> 16) & 0xFF), ((cnt2 >> 8) & 0xFF), ((cnt2) & 0xFF), ntohs(route_hdr_ptr->port),num_domains);
			}

			ptr = pktData + LBMR_REMOTE_DOMAIN_ROUTE_HDR_SZ;
			for ( cnt=0; cnt < num_domains; cnt++ ) {
				/* To DO: Check for the overall size of the packet frame to limit output for corrupt packets */
				char remote_domain[12];
				TS_printf(PRINT_DRO, "\n Route to Domain: %i ",  ntohl(*(lbm_uint32_t *) ptr) );
				if( options.stats == 1 ){
					sprintf(remote_domain,"[%i]", ntohl(*(lbm_uint32_t *) ptr));
					strcat(source_name, remote_domain);
				}
				ptr += sizeof(lbm_uint32_t);

			}
			if( options.stats == 1 ){
				stats_domains++;
				if(prev_num_domains != num_domains){	
					TS_printf(PRINT_LBMR,"\n%s%s", source_name, options_str);
					TS_stats_DRO(source_name, options_str, tv, 1); /* Store the change in accessible domains */
					prev_num_domains = num_domains;
				}
			}
		    }
	  	  break;
	  	  case LBMR_HDR_EXT_TYPE_REMOTE_CONTEXT_INFO:
		    {
			uint16_t ctx_flags = 0;
			uint16_t ctx_len = 0;
			uint16_t num_recs = 0;
			uint16_t rec_len = 0;
			uint16_t len_chk = 0;
			char *ctx_type = NULL;

			TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_EXT_TYPE_REMOTE_CONTEXT_INFO");
			ptr = pktData;
			lbmr_rctxinfo_ptr = (lbmr_rctxinfo_t *) ptr;
			lbmr_rctxinfo_rec_t * lbmr_rctxinfo_rec_t_ptr;
			
			stats_ctxinfo++;
			TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_ptr->ver_type = %i", lbmr_rctxinfo_ptr->ver_type);
			TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_ptr->ext_type = %i", lbmr_rctxinfo_ptr->ext_type);
			TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_ptr->len = %i", ntohs(lbmr_rctxinfo_ptr->len));
			num_recs = ntohs(lbmr_rctxinfo_ptr->num_recs);
			TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_ptr->num_recs = %i", num_recs);
			
			ptr += LBMR_RCTXINFO_SZ;
			lbmr_rctxinfo_rec_t_ptr=(lbmr_rctxinfo_rec_t *) ptr;
			ctx_flags = ntohs(lbmr_rctxinfo_rec_t_ptr->flags);
			ctx_len = ntohs(lbmr_rctxinfo_rec_t_ptr->len);
			TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_t_ptr->len = %i; lbmr_rctxinfo_rec_t_ptr->flags = %i  ", ctx_len, ctx_flags);
			if((ctx_flags & LBMR_RCTXINFO_REC_FLAG_QUERY) != 0) {
				ctx_type = "CTX_QUERY";
				TS_printf(PRINT_UMP, ",CTX_QUERY");
			} else {
				ctx_type = "CTX_AD";
				TS_printf(PRINT_UMP, ",CTX_AD");
			}
			snprintf(source_name,LBM_MSG_MAX_SOURCE_LEN, "%s",ctx_type);

			lenparsed = 0;
			len_chk = ntohs(lbmr_rctxinfo_ptr->len) - (LBMR_RCTXINFO_SZ + LBM_RCTXINFO_REC_SZ);
			for ( ctr = 0 ;(ctr < num_recs) && (lenparsed < len_chk); ctr++){
			  TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_t_ptr->len = %i; ctr = %i lenparsed=%i", ctx_len, ctr, lenparsed);
                          lbm_uint8_t opt_len;
                          lbmr_rctxinfo_rec_opt_t * opt;


			  ptr += LBM_RCTXINFO_REC_SZ;
			  opt =  (lbmr_rctxinfo_rec_opt_t *) ptr;
			  TS_printf(PRINT_UMP, "\n opt->type = %i,  opt->len = %i, opt->flags = %i", opt->type, opt->len, ntohs(opt->flags));
                          rec_len = ctx_len - LBM_RCTXINFO_REC_SZ;


                          for (opt_len = 0; opt_len < rec_len; opt=(lbmr_rctxinfo_rec_opt_t *)ptr){
                           switch (opt->type) {
				lbmr_rctxinfo_rec_address_opt_t * lbmr_rctxinfo_rec_address_opt_t_ptr;
                                lbmr_rctxinfo_rec_instance_opt_t * lbmr_rctxinfo_rec_instance_opt_t_ptr;
                		lbmr_rctxinfo_rec_odomain_opt_t * lbmr_rctxinfo_rec_odomain_opt_t_ptr;
                                lbmr_rctxinfo_rec_name_opt_t * lbmr_rctxinfo_rec_name_opt_t_ptr;

                                case LBMR_RCTXINFO_OPT_ADDRESS_TYPE:
                                        if ((opt->len != LBMR_RCTXINFO_REC_ADDRESS_OPT_SZ)) {
						TS_printf(PRINT_ERROR,
							"\n ERROR! lenparsed(%i): opt->len(%i) != LBMR_RCTXINFO_REC_ADDRESS_OPT_SZ(%i)"
							, lenparsed, opt->len, LBMR_RCTXINFO_REC_ADDRESS_OPT_SZ  );
                                                return 1450;
                                        }
                                        lbmr_rctxinfo_rec_address_opt_t_ptr = (lbmr_rctxinfo_rec_address_opt_t *) ptr;

					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->type = %i", lbmr_rctxinfo_rec_address_opt_t_ptr->type );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->len = %i", lbmr_rctxinfo_rec_address_opt_t_ptr->len );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->flags = %i", ntohs(lbmr_rctxinfo_rec_address_opt_t_ptr->flags) );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->domain_id= %i", ntohl(lbmr_rctxinfo_rec_address_opt_t_ptr->domain_id));
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->ip = %x", ntohl(lbmr_rctxinfo_rec_address_opt_t_ptr->ip) );
								TS_pretty_print_addr(PRINT_UMP,  ntohl(lbmr_rctxinfo_rec_address_opt_t_ptr->ip) );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->port = %i", ntohs(lbmr_rctxinfo_rec_address_opt_t_ptr->port) );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_address_opt_t_ptr->res = %i", ntohs(lbmr_rctxinfo_rec_address_opt_t_ptr->res) );
					
					stats_umps++;
					if( (app_maj_ver > 5) && ( options.stats == 1 ) ){

						//Store should send context ads on startup and query
						TS_printf(PRINT_UMP,"\nApp Major version: %i, Assuming Context Adv from UMESTORED\n", app_maj_ver);
					
						cnt = ntohl(lbmr_rctxinfo_rec_address_opt_t_ptr->ip);
						snprintf(temp_buff,LBM_MSG_MAX_SOURCE_LEN, "umestoredCtxAd[%i.%i.%i:%i:%i][%i]%s",
							((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),
							ntohs(lbmr_rctxinfo_rec_address_opt_t_ptr->port), ntohl(lbmr_rctxinfo_rec_address_opt_t_ptr->domain_id), ctx_type);
						strncat(source_name,temp_buff,LBM_MSG_MAX_SOURCE_LEN);
					}
			    		ptr+=lbmr_rctxinfo_rec_address_opt_t_ptr->len;
					opt_len += lbmr_rctxinfo_rec_address_opt_t_ptr->len;
                                        break;
                                case LBMR_RCTXINFO_OPT_INSTANCE_TYPE:
					TS_printf(PRINT_UMP," LBMR_RCTXINFO_OPT_INSTANCE_TYPE:");
                                        lbmr_rctxinfo_rec_instance_opt_t_ptr = (lbmr_rctxinfo_rec_instance_opt_t *) ptr;
					TS_printf(PRINT_VERBOSE, "\n lbmr_rctxinfo_rec_instance_opt_t_ptr->type = %i", lbmr_rctxinfo_rec_instance_opt_t_ptr->type );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_instance_opt_t_ptr->len = %i", lbmr_rctxinfo_rec_instance_opt_t_ptr->len );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_instance_opt_t_ptr->flags = %i", ntohs(lbmr_rctxinfo_rec_instance_opt_t_ptr->flags) );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_instance_opt_t_ptr->instance = " );
							 for (cnt =0; cnt < LBM_CONTEXT_INSTANCE_BLOCK_SZ; cnt++){
							 	TS_printf(PRINT_VERBOSE|PRINT_UMP, "%x", lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[cnt]);
							 }
					if( (app_maj_ver > 5) && ( options.stats == 1 ) ){
						//Remote Store sends these out
						snprintf(temp_buff, LBM_MSG_MAX_SOURCE_LEN, ",Instance:[%x%x%x%x%x%x%x%x]",
							 lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[0],lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[1],
							lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[2],lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[3],
							lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[4],lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[5],
							lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[6],lbmr_rctxinfo_rec_instance_opt_t_ptr->instance[7] );
						strncat(source_name,temp_buff,LBM_MSG_MAX_SOURCE_LEN);
					}
					ptr+=lbmr_rctxinfo_rec_instance_opt_t_ptr->len;
					opt_len += lbmr_rctxinfo_rec_instance_opt_t_ptr->len;
					break;
                                case LBMR_RCTXINFO_OPT_ODOMAIN_TYPE:
					TS_printf(PRINT_UMP," \n LBMR_RCTXINFO_OPT_ODOMAIN_TYPE");
                                        lbmr_rctxinfo_rec_odomain_opt_t_ptr = (lbmr_rctxinfo_rec_odomain_opt_t *) ptr;
					TS_printf(PRINT_VERBOSE, "\n lbmr_rctxinfo_rec_odomain_opt_t_ptr->type = %i", lbmr_rctxinfo_rec_odomain_opt_t_ptr->type );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_odomain_opt_t_ptr->len = %i", lbmr_rctxinfo_rec_odomain_opt_t_ptr->len );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_odomain_opt_t_ptr->flags = %i", ntohs(lbmr_rctxinfo_rec_odomain_opt_t_ptr->flags) );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_odomain_opt_t_ptr->domain_id = %i", ntohl(lbmr_rctxinfo_rec_odomain_opt_t_ptr->domain_id) );
					stats_umps++;
					if( (app_maj_ver > 5) && ( options.stats == 1 ) ){
						snprintf(temp_buff,LBM_MSG_MAX_SOURCE_LEN-1, ",RCtxInfoDomain:%i",ntohl(lbmr_rctxinfo_rec_odomain_opt_t_ptr->domain_id));
						strncat(source_name,temp_buff,LBM_MSG_MAX_SOURCE_LEN);
					}
					ptr+=lbmr_rctxinfo_rec_odomain_opt_t_ptr->len;
					opt_len += lbmr_rctxinfo_rec_odomain_opt_t_ptr->len;
					break;
                                case LBMR_RCTXINFO_OPT_NAME_TYPE:
					TS_printf(PRINT_UMP," LBMR_RCTXINFO_OPT_NAME_TYPE:");
                                        lbmr_rctxinfo_rec_name_opt_t_ptr = (lbmr_rctxinfo_rec_name_opt_t *) ptr;
					TS_printf(PRINT_VERBOSE, "\n lbmr_rctxinfo_rec_name_opt_t_ptr->type = %i", lbmr_rctxinfo_rec_name_opt_t_ptr->type );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_name_opt_t_ptr->len = %i", lbmr_rctxinfo_rec_name_opt_t_ptr->len );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_name_opt_t_ptr->flags = %i", ntohs(lbmr_rctxinfo_rec_name_opt_t_ptr->flags) );
					TS_printf(PRINT_UMP, "\n lbmr_rctxinfo_rec_name: %s\n" ,ptr+LBMR_RCTXINFO_REC_NAME_OPT_BASE_SZ );
					if( (app_maj_ver > 5) && ( options.stats == 1 ) ){
						snprintf(temp_buff,LBM_MSG_MAX_SOURCE_LEN-1, ",Store_Name:%s",ptr+LBMR_RCTXINFO_REC_NAME_OPT_BASE_SZ);
						strncat(source_name,temp_buff,LBM_MSG_MAX_SOURCE_LEN);
					}
					ptr+=lbmr_rctxinfo_rec_name_opt_t_ptr->len;
					opt_len += lbmr_rctxinfo_rec_name_opt_t_ptr->len;
                                        break;
                                default:
					TS_printf(PRINT_UMP|PRINT_ERROR, "\n NOT YET IMPLEMENTED LBMR_RCTXINFO_OPT_?_TYPE:%i", opt->type );
                                        return 1460;
                              }/* End switch */
			    } /* End for */

			   /* Save away the UMP Stats */
			   cnt = ntohl(fromaddr->sin_addr.s_addr);
			   snprintf(temp_buff,LBM_MSG_MAX_SOURCE_LEN-1, "LBMR:%i.%i.%i.%i:%i,%s",
				((cnt >> 24) & 0xFF), ((cnt >> 16) & 0xFF), ((cnt >> 8) & 0xFF), ((cnt) & 0xFF),ntohs(fromaddr->sin_port), options_str);
			   TS_printf(PRINT_UMP,"%s %s", source_name, temp_buff);
			   TS_stats_UMP(source_name, temp_buff, tv, TS_save_all_entries);

			   lenparsed += opt_len;
                           ptr += opt_len;
                        }
		  }
	  	  break;
	  	  default:
			TS_printf(PRINT_ERROR, "\n ERROR! Undefined extended type ");
			return 1500;
		  break;
		}
	  break;
	  case LBMR_HDR_TYPE_UCAST_RCV_ALIVE:
	  {
		TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_TYPE_UCAST_RCV_ALIVE");
		if ((lbm_hdr_ptr->tqrs!=0)  || (num_tirs!=0)) { /* To do: check this */
			TS_printf(PRINT_ERROR, "\n ERROR! TIRs and TQRS, included with LBMR_HDR_TYPE_UCAST_RCV_ALIVE: ");
			return 3000;
		}
		stats_lbmrd_keepalive++;
	  }
	  break;
	  case LBMR_HDR_TYPE_UCAST_SRC_ALIVE:
		TS_printf(PRINT_VERBOSE, "\n LBMR type ....is... LBMR_HDR_TYPE_UCAST_SRC_ALIVE");
		if ((lbm_hdr_ptr->tqrs!=0)  || (num_tirs!=0)) { /* To do: check this */
			TS_printf(PRINT_ERROR, "\n ERROR! TIRs and TQRS, included with LBMR_HDR_TYPE_UCAST_SRC_ALIVE: ");
			return 3001;
		}
		stats_lbmrd_keepalive++;
		
	  break;
	  default:
		TS_printf(PRINT_ERROR, "\n Error: Invalid LBMR Type  0x%x ", LBMR_HDR_TYPE(lbm_hdr_ptr->ver_type));
		return 1600;
	  break;
	}/* End switch */

	if( unicastTR == true ) {
		TS_printf(PRINT_VERBOSE,"\n LBMRD-> ");
		TS_lbmrd_keepalive_received=99;
		stats_lbmrd_keepalive++;
	}

	fflush(stdout);
	return 0;
}/* DecodeLbmrPacket */

void * TS_stats_dump_thread( void * interval ){
	uint32_t dump_ivl;
	uint32_t clear_ivl;
	uint32_t min_ivl;

	dump_ivl = options.stats_ivl;
	clear_ivl = options.clearstats_ivl;

	if((clear_ivl>dump_ivl) || (clear_ivl==0)){
		min_ivl = dump_ivl;
	} else {
		min_ivl = clear_ivl;
	}

	TS_printf(PRINT_VERBOSE, "\nStarting TS_stats_dump_thread, options( min_ivl[%i], dump_ivl[%i], clear_ivl[%i] )", min_ivl, dump_ivl, clear_ivl);
	fflush (stdout);

	while(1){
	fflush (stdout);
		sleep(min_ivl);
		dump_ivl  -= min_ivl;
		clear_ivl -= min_ivl;
		
		if( dump_ivl < min_ivl){
			TS_print_stored_stats();
			dump_ivl = options.stats_ivl;
		}
		if(clear_ivl < min_ivl){
			TS_clear_stored_stats();
			clear_ivl = options.clearstats_ivl;
		}
	}/* end while() */
}

void *
lbmrd_keepalive( void *uniAddr )
{

	TRaddr_t myAddr = *((TRaddr_t *)(uniAddr));
	int res1, res2;
	struct timeval tv;
	struct sockaddr_in lbmrdAddr;

	struct keepalive {
		uint8_t  ver_type;
		uint8_t  tqrs;
		uint16_t tirs;
	} kaRcvBuf, kaSrcBuf;
	
	TS_printf(PRINT_LBMRD, "pthread - uniAddr = 0x%x\n", uniAddr );


	kaRcvBuf.ver_type = 2;
	kaRcvBuf.tqrs = 0;
	kaRcvBuf.tirs = 0;
	
	kaSrcBuf.ver_type = 3;
	kaSrcBuf.tqrs = 0;
	kaSrcBuf.tirs = 0;
	
	lbmrdAddr.sin_family = AF_INET;
	lbmrdAddr.sin_addr.s_addr = myAddr.addr;
	lbmrdAddr.sin_port = myAddr.port;
	
	TS_printf(PRINT_LBMRD, "myAddr.sockfd = %d\n", myAddr.sockfd );

	while( true )
	{

		gettimeofday(&tv, NULL);
		if(TS_lbmrd_keepalive_received == 1 || TS_lbmrd_keepalive_received == 0 ){
			TS_printf(PRINT_LBMRD|PRINT_VERBOSE,"\nLBMRD: Response/LBMR received :  %i.%i\n", tv.tv_sec, tv.tv_usec);
			TS_lbmrd_keepalive_received = 2;
		}else if(TS_lbmrd_keepalive_received > 90){
			TS_printf(PRINT_LBMRD, "LBMRD: Received TR instead of keepalive");
			TS_lbmrd_keepalive_received = 2;
		} else {
			TS_printf(PRINT_ERROR|PRINT_LBMRD|PRINT_WARNING|PRINT_VERBOSE,"\nWARNING! LBMRD No response[%i] :  %i.%i\n",TS_lbmrd_keepalive_received, tv.tv_sec, tv.tv_usec);
		}
			
		usleep( 250000 );

		res1 = sendto( myAddr.sockfd, (const void *)(&kaRcvBuf), 4, 0, (struct sockaddr *)(&lbmrdAddr),
		        sizeof(myAddr) );

		usleep( 250000 );

		res2 = sendto( myAddr.sockfd, (const void *)(&kaSrcBuf), 4, 0, (struct sockaddr *)(&lbmrdAddr),
		        sizeof( myAddr) );
		
		gettimeofday(&tv, NULL);
		TS_printf(PRINT_LBMRD|PRINT_VERBOSE, "\n[%d-%d] 2 LBMRD keepalives sent at %i.%i\n", res1, res2, tv.tv_sec, tv.tv_usec);
	}
}

int
main (int argc, char * argv[])
{
	socklen_t fromlen;
	struct sockaddr_in fromaddr;
	int rsock = -1;
	int val;
	int cnt;
	int nBytes;
	int frag_offset;
	int32_t TS_process_pkts_stop = 0xFFFFFFFF;
	int32_t TS_process_pkts_start = 0;
	uint8_t  *U8_ptr;
	uint8_t  *UDP_datagram_ptr;
	uint16_t mf_flag;
	char *endptr;
	uint32_t stats_bytes;
	uint32_t stats_pktCount;
	uint32_t stats_bytespersec ;
	struct timeval tv;
	struct timeval tv_prev;
	pcap_hdr_t *pHdr;
	char found_it;

	memset( (void *)&tv, 0, sizeof(tv) );
	memset( (void *)&tv_prev, 0, sizeof(tv_prev) );

	stats_bytes = 0;
	stats_pktCount=1;

	progname = argv[0];

	options.pcapFile = PCAP_DEFAULT;
	options.pcapFile = NULL; // Dont save .pcap automatically
	options.raw = 0;
	options.readpcapFile = NULL;
	options.stats = 0;
	options.found_lvl = PRINT_STATS;
	pcapHdr = (pcaprec_hdr_t *)dataBuf;


	ipHdr = (struct iphdr *)BUF_OFFSET(pcapHdr, sizeof(*pcapHdr));
	udpHdr = (struct udphdr *)BUF_OFFSET(ipHdr, sizeof(*ipHdr));

	/* "m:u:r:p:vs:fF:l:S:E:C:U:h" */
	while ((val = getopt_long(argc, argv, optStr, longopts, NULL)) != -1) {
		switch (val) {
		case 'u':
			{
				unicastTR = true;
				char *rdAddr, *port, *iface;
				rdAddr = optarg;
				port = strchr(rdAddr, ':');
				if (port == NULL) {
					port = "15381";
					iface = "0";
				} else {
					*port = '\0';
					port++;
					iface = strchr(port, ':');
					if (iface == NULL) {
						iface = "0";
					} else {
						*iface = '\0';
						iface++;
					}
				}
				TS_printf(PRINT_LBMRD, "Unicast %s:%s:%s\n", rdAddr, port, iface );
				if (addAddrList(rdAddr, port, iface, 0) != 0) {
					perror(optarg);
					Usage(progname);
					exit(-1);
				}
			}
			break;

		case 'm':
			{
				char *group, *port, *iface;
				mcastTR = true;
				group = optarg;
				port = strchr(group, ':');
				if (port == NULL) {
					port = "12965";
					iface = "0";
				} else {
					*port = '\0';
					port++;
					iface = strchr(port, ':');
					if (iface == NULL) {
						iface = "0";
					} else {
						*iface = '\0';
						iface++;
					}
				}
				if (addAddrList(group, port, iface, 1) != 0) {
					perror(optarg);
					Usage(progname);
					exit(-1);
				}
			}
			break;

		case 'p':
			options.pcapFile = optarg;
			break;
		case 'r':
			/* to do: check that optarg is not clobbered, otherwise strcpy */
			options.readpcapFile = optarg;
			break;
		case 's':
			options.stats = 1;
			options.stats_ivl = atol(optarg);
			TS_current_log_level |= PRINT_STATS;
			printf("\nSetting Stats : Stats interval = %i secs, TS_current_log_level = %x\n", options.stats_ivl, TS_current_log_level);
			break;
		case 'v':
			TS_current_log_level = PRINT_M;
			printf("\nSetting Verbose: TS_current_log_level = %x\n", TS_current_log_level);
			break;
		case 'l':
			TS_current_log_level = (uint32_t) strtol(optarg, &endptr, 16);
			if (endptr == optarg) {
               			TS_printf(PRINT_ERROR, "ERROR: Case(-l) could not parse MASK digits \n");
			}
			printf("\nSetting loglevel: TS_current_log_level = 0x%x\n", TS_current_log_level);
			break;
		case 'h':
			Usage(progname);
			exit(0);
			break;
		case 'S':
			TS_process_pkts_start=(int32_t) atol(optarg);
			printf("\nSetting TS_process_pkts_start = %i\n", TS_process_pkts_start);
			break;
		case 'E':
			TS_process_pkts_stop=(int32_t) atol(optarg);
			printf("\nSetting TS_process_pkts_stop = %i\n", TS_process_pkts_stop);
			break;
		case 'f':
			options.found_lvl = PRINT_LBMR;
			break;
		case 'F':
			TS_max_fileline_count = (int32_t) atol(optarg);
			TS_max_fileline_count *= 1000000;
			printf("\nSetting TS_max_fileline_count = %i\n", TS_max_fileline_count);

			if (mkdir("TR_LOGS", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0 ){
				perror("\n\tWARNING! Could not create directory : TR_LOGS ");
			}
				
				
			TS_output_file=fopen("TR_LOGS/x0", "w");
           		if (TS_output_file == NULL){
				perror("\n\tERROR! Could not open output file: x0 ");
				exit(EXIT_FAILURE);
			}
			break;
		case 'C':
			options.clearstats_ivl = (int32_t) atol(optarg);
			printf("\nClearing Stats interval = %i secs, TS_current_log_level = %x\n", options.clearstats_ivl, TS_current_log_level);
			break;
		case 'U':
			options.uniquestatsonly = (uint32_t) atol(optarg);
			if (options.uniquestatsonly != 0){
				TS_save_all_entries = 0;
				printf("\nAccumulating only unique stats: TS_save_all_entries %i", TS_save_all_entries);
			}else{
				TS_save_all_entries = 1;
				printf("\nAccumulating all instances in stats: TS_save_all_entries %i", TS_save_all_entries);
			}
			break;
		}
	}

	if (AddrList == NULL && options.readpcapFile==NULL) {
		addAddrList("224.9.10.11", "12965", "0", 1);
	}


	if (options.readpcapFile) {
		inFd = open(options.readpcapFile, O_RDONLY, 0644);
		if (inFd < 0) {
			perror(options.readpcapFile);
			exit(errno);
		}
	} else {
		if (options.pcapFile) {
		  outFd = open(options.pcapFile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		  if (outFd < 0) {
			perror(options.pcapFile);
			exit(errno);
		  }
		  // Fill in the IP and UDP header fields
		  //initHeaders(ipHdr, udpHdr);
		  initHeaders();
		  pcapInitFile(outFd);
	        }
		rsock = AddrList->sockfd;
	}

	if( unicastTR == true )
	{
		pthread_t threadId;
		pthread_attr_t attr;
		int result;

		// start a thread which will send unicast KAs to the TR daemon
		pthread_attr_init( &attr );

		TS_printf(PRINT_VERBOSE, "AddrList=0x%x\n", (void *)(AddrList) );

		TS_lbmrd_keepalive_received = 2;
		result = pthread_create( &threadId, &attr, lbmrd_keepalive, (void *)(AddrList) );
		
		if( result != 0 )
		{
			perror( "Could not create thread to send Unicast TR Keepalives");
			exit( result );
		}
	}

	if( (options.stats_ivl != 0) && (options.readpcapFile == NULL)){
		pthread_t threadId;
		pthread_attr_t attr;
		int result;

		// Spawn thread that prints stats every stats_ivl
		pthread_attr_init( &attr );
		result = pthread_create( &threadId, &attr, &TS_stats_dump_thread, &options.stats_ivl );
		if( result != 0 )
		{
			TS_printf(PRINT_ERROR, "\nCould not create thread to save stored stats");
			perror( "Could not create thread to save stored stats ");
			exit( result );
		} else {
			TS_printf(PRINT_VERBOSE, "\nStats thread: initialized successfully ");
		}
	}

	fflush (stdout);
	if(inFd > 0){ /* This section reads in pcap data and performs some analysis */
		TS_printf(PRINT_VERBOSE, "\n Read PCAP header ");
		if (read(inFd, dataBuf, sizeof(pcap_hdr_t)) < 0) {
			perror("read (pcapHdr)");
			exit(errno);
		}
		pHdr = (pcap_hdr_t *)dataBuf;
		TS_printf(PRINT_VERBOSE, "\npHdr->magic_number (%x) = %x",PCAP_MAGIC, pHdr->magic_number);
		TS_printf(PRINT_VERBOSE, "\npHdr->version_major (%x) = %x", PCAP_MAJOR, pHdr->version_major);
		TS_printf(PRINT_VERBOSE, "\npHdr->version_minor (%x) = %x",  PCAP_MINOR, pHdr->version_minor);
		TS_printf(PRINT_VERBOSE, "\npHdr->thiszone (%x) = %x", 0, pHdr->thiszone);
		TS_printf(PRINT_VERBOSE, "\npHdr->sigfigs (%x) = %x",  pHdr->sigfigs);
		TS_printf(PRINT_VERBOSE, "\npHdr->snaplen (%x) = %x", MAX_RBUF,pHdr->snaplen);
		TS_printf(PRINT_VERBOSE, "\npHdr->network (%x) = %x", PCAP_LINK_RAW,pHdr->network);
		
		/* Initialize the time */
		pcapHdr = (pcaprec_hdr_t *)dataBuf;

		int start_flag = 0;
		while (read(inFd, dataBuf, sizeof(pcaprec_hdr_t)) > 0) {
 		
			tv.tv_sec = pcapHdr->ts_sec;
			tv.tv_usec = pcapHdr->ts_usec;

			if (start_flag==0){
				tv_prev.tv_sec = tv.tv_sec;
				tv_prev.tv_usec = tv.tv_usec;
				start_flag=1;
			}

			printf("\nPKT:%d %i.%i ", stats_pktCount, pcapHdr->ts_sec, pcapHdr->ts_usec);

			/* Read and parse the rest of the PCAP */
			pcapHdr = (pcaprec_hdr_t *)dataBuf;
			TS_printf(PRINT_VERBOSE|PRINT_STATS, "\nPKT:%d %i.%i ", stats_pktCount, pcapHdr->ts_sec, pcapHdr->ts_usec);
			TS_printf(PRINT_PCAP, "\npcapHdr->ts_sec.ts_usec = %i.%i", pcapHdr->ts_sec, pcapHdr->ts_usec);

			if ( pcapHdr->incl_len != pcapHdr->orig_len){
				TS_printf(PRINT_VERBOSE, "\n PCAP truncated by snaplength ");
			}
	
			/* Read the packet payload */
			nBytes = pcapHdr->incl_len;
			if (read(inFd, dataBuf, pcapHdr->incl_len) < 0) {
				perror("read (pcapHdr)");
				exit(errno);
			}
	
			TS_printf(PRINT_PCAP, "\npcapHdr->incl_len = %i", pcapHdr->incl_len);
			TS_printf(PRINT_PCAP, "\npcapHdr->orig_len = %i", pcapHdr->orig_len);
		 	TSdump(PRINT_DEBUG, (char *)dataBuf, nBytes);	
	
	
			if(options.stats==1){
				TS_printf(PRINT_DEBUG, "\ntv %i.%i %i.%i ", tv_prev.tv_sec,tv_prev.tv_usec,tv.tv_sec, tv.tv_usec);
				if(tv.tv_sec > tv_prev.tv_sec){
					TS_printf(PRINT_STATS, "\n %d KBps\n", stats_bytespersec/1000);
					tv_prev.tv_sec = tv.tv_sec;
					tv_prev.tv_usec = tv.tv_usec;
					stats_bytespersec = 0;
				}
				stats_bytespersec += nBytes;
				stats_bytes += nBytes;
			}
			/* stats_pktCount++; */

			// Check if these packets should be skipped
			if ( (stats_pktCount < TS_process_pkts_start) || (stats_pktCount > TS_process_pkts_stop )) {
				TS_printf(PRINT_DEBUG,"\n [%i]-[%i] skipping packet : %i", TS_process_pkts_start, TS_process_pkts_stop, stats_pktCount);
			} else {


			// Pcap HDR. Determine if an ethernet header is present
			U8_ptr = (uint8_t *) dataBuf;
			ethHdr = (struct ethhdr *) U8_ptr;
			ipHdr = (struct iphdr *)U8_ptr;
			TS_printf(PRINT_PCAP,"\nethHdr->h_proto:%x",htons(ethHdr->h_proto));
			val = htons(ethHdr->h_proto);
			if(htons(ethHdr->h_proto) == ETH_P_IP){
				TS_printf(PRINT_PCAP,"ethHdr->h_dest:0x"); TSdump(PRINT_PCAP, (char *)&ethHdr->h_dest[0] , ETH_ALEN);
				TS_printf(PRINT_PCAP,"ethHdr->h_source:0x"); TSdump(PRINT_PCAP, (char *)&ethHdr->h_source[0] , ETH_ALEN);
				U8_ptr = (uint8_t *) BUF_OFFSET(dataBuf, ETH_HLEN);
			} else {
				if(val == ETH_P_IP){
					/* Raw PCAP ... No ethernet header */
					printf("\n Raw PCAP ... No ethernet header .. probably need to fix ");
				} else {
						/* Try randomly to get to the IP header   ... not exactly robust code; TO DO: re-visit and check raw PCAP still works */
						for (  cnt=0, found_it=0; (cnt < 48) && (found_it ==0); cnt++ ){
							U8_ptr++;
							val  = htons(*(uint16_t *) U8_ptr);
							ipHdr = (struct iphdr *)U8_ptr;
							if( (val == ETH_P_IP) && (*(U8_ptr+2)==0x45) ){
								found_it = 1;
							}
						}
						if(val != ETH_P_IP || cnt == 48 ){
							TS_printf(PRINT_WARNING,"\nERROR!: Typical IP Ethernet frame not detected: Pkt %u",stats_pktCount);
						} else {
							U8_ptr += 2; /* Advance past h_proto */
						}
				}
			}

#if 0
printf("\n found it:%i", found_it);
printf("\n Dumping the databuf, buffer ");
TSdump(PRINT_PCAP, dataBuf, 32);
printf("\n Dumping the ethHdr, buffer ");
TSdump(PRINT_PCAP, ethHdr, 16);
printf("\n Dumping the U8_ptr data buffer ");
TSdump(PRINT_PCAP, U8_ptr, 16);
exit(1);
#endif
			ipHdr = (struct iphdr *)U8_ptr;
			udpHdr = (struct udphdr *)BUF_OFFSET(ipHdr, sizeof(*ipHdr));


			 TS_printf(PRINT_PCAP, "\nipHdr->saddr = ");
			 TS_pretty_print_addr(PRINT_PCAP,  ntohl(ipHdr->saddr));
			 TS_printf(PRINT_PCAP, "\nipHdr->tot_len = %i", ntohs(ipHdr->tot_len));
			 TS_printf(PRINT_PCAP, "\nipHdr->daddr = ");
			 TS_pretty_print_addr(PRINT_PCAP,  ntohl(ipHdr->daddr));
			 TS_printf(PRINT_PCAP, "\nipHdr->check = 0x%x", ntohs(ipHdr->check));
			 TS_printf(PRINT_PCAP, "\nipHdr->ttl = %x", ipHdr->ttl);
			 TS_printf(PRINT_PCAP, "\nipHdr->protocol = %x", ipHdr->protocol);
			 TS_printf(PRINT_PCAP, "\nipHdr->tos = %x ", ipHdr->tos);
			 TS_printf(PRINT_PCAP, "\nipHdr->id = %x ", ntohs(ipHdr->id));
			 TS_printf(PRINT_PCAP, "\nipHdr->frag_off = %x (%i) ", ntohs(ipHdr->frag_off), ntohs(ipHdr->frag_off));

			 TS_printf(PRINT_PCAP, "\nipHdr->ihl = %i", ipHdr->ihl);
			 TS_printf(PRINT_PCAP, "\nipHdr->version = %i", ipHdr->version);
			 TS_printf(PRINT_PCAP, "\nipHdr->id = %i", ipHdr->id);
	
			 nBytes = ntohs(ipHdr->tot_len) - sizeof(*ipHdr) ;
	
			 fflush (stdout);

			if (ipHdr->protocol == IPPROTO_UDP){

			 TS_printf(PRINT_PCAP, "\nudpHdr->source = %i", ntohs(udpHdr->source));
			 TS_printf(PRINT_PCAP, "\nudpHdr->dest = %i", ntohs(udpHdr->dest));
			 TS_printf(PRINT_PCAP, "\nudpHdr->len = %i",ntohs(udpHdr->len));
			 TS_printf(PRINT_PCAP, "\nudpHdr->check = 0x%x", ntohs(udpHdr->check));
	
			 if (ntohs(ipHdr->frag_off) != IP_DF && (ntohs(ipHdr->frag_off) != 0)){
	
				TS_printf(PRINT_DEBUG, "\n fragment detected ");
				mf_flag = (ntohs(ipHdr->frag_off) & IP_MF);
				frag_offset = 8*(ntohs(ipHdr->frag_off) & ~IP_MF);
				/* Assemble the fragments: */
			  	U8_ptr = (uint8_t *)(BUF_OFFSET(ipHdr, sizeof(*ipHdr)));
	
				fromaddr.sin_addr.s_addr = ipHdr->saddr;
				fromaddr.sin_port = udpHdr->source;

			 	TS_printf(PRINT_VERBOSE, " \nSRC:"); TS_pretty_print_addr(PRINT_VERBOSE,  ntohl(ipHdr->saddr));
			 	TS_printf(PRINT_VERBOSE, " ---> ");
			 	TS_printf(PRINT_VERBOSE, " DST:"); TS_pretty_print_addr(PRINT_VERBOSE,  ntohl(ipHdr->daddr));
	
				val = TS_assemble_udp_frags( ntohs(ipHdr->id), U8_ptr, nBytes, frag_offset, mf_flag, tv, &fromaddr);
				
			 } else {
			 	TS_printf(PRINT_VERBOSE, " \nSRC:"); TS_pretty_print_addr(PRINT_VERBOSE,  ntohl(ipHdr->saddr));
			 	TS_printf(PRINT_VERBOSE, ":%i ",ntohs(udpHdr->source));
			 	TS_printf(PRINT_VERBOSE, " ---> ");
			 	TS_printf(PRINT_VERBOSE, " DST:"); TS_pretty_print_addr(PRINT_VERBOSE,  ntohl(ipHdr->daddr));
			 	TS_printf(PRINT_VERBOSE, ":%i ",ntohs(udpHdr->dest));
	
				UDP_datagram_ptr = (uint8_t *) BUF_OFFSET(ipHdr, sizeof(*ipHdr) + sizeof(*udpHdr));
	#if DEBUG
				TS_printf(PRINT_DEBUG, "\n UDP: nBytes = %i", nBytes);
				TSdump(PRINT_DEBUG, UDP_datagram_ptr, nBytes);
	#endif
				fromaddr.sin_addr.s_addr = ipHdr->saddr;
				fromaddr.sin_port = udpHdr->source;
				/* Check if its LBMR */
				val = DecodeLbmrPacket(UDP_datagram_ptr, &fromaddr, nBytes-sizeof(*udpHdr), &tv, AddrList);
				  if ( val == 0) {
					stats_lbmr_pktCount++;
					TS_printf(options.found_lvl, "\n LBMR packet found. stats_lbmr_pktCount: %i \n", stats_lbmr_pktCount);
				  } else {
					/* to do: Check for LBTRM or LBTRU */
					TS_printf(PRINT_WARNING, "\n UNKNOWN UDP packet found. stats_lbmr_pktCount unchanged: %i \n", stats_lbmr_pktCount);
					stats_nonlbmr_pktCount++;
				  }
			  }
				
			} /* End of if UDP */
			else {
				TS_printf(PRINT_WARNING, "\n WARNING! Skipping Non-UDP packet");
			}
			stats_pktCount++;
			TS_printf(PRINT_VERBOSE, "\n Done. Packet Count = %i ", stats_pktCount);
		 } /* Check to process packet */
	    }/* End while() */
	    if(options.stats==1){
		TS_print_stored_stats();
	    }
            if (TS_output_file != NULL){
			printf("\n Closing output file ");
                	fclose(TS_output_file);
	    }
	    printf("\n Done. ");
	    exit(0);
	}

	while (1) {
		int pcapSize;

		fromlen = sizeof(struct sockaddr_in);

		/* need a poll here */
		nBytes = recvfrom(rsock, captureStart, MAX_RBUF, 0, (struct sockaddr *)&fromaddr, &fromlen);

		if (nBytes < 0) {
			if (errno == EINTR) {
				fprintf(stderr, "Interrupted - received %d packets\n", stats_pktCount);
				exit(EINTR);
			}
			perror("recvfrom");
			exit(errno);
		}

		stats_pktCount++;
		gettimeofday(&tv, NULL);
		val = DecodeLbmrPacket(captureStart, &fromaddr, nBytes, &tv, AddrList);
		if ( val == 0) {
			stats_lbmr_pktCount++;
			if( unicastTR == true ) {
				TS_printf(options.found_lvl, "\n LBMRD LBMR packet found. stats_lbmr_pktCount: %i \n", stats_lbmr_pktCount);
			} else {
				TS_printf(options.found_lvl, "\n LBMR packet found. stats_lbmr_pktCount: %i \n", stats_lbmr_pktCount);
			}
		} else if( val == 99999 ){
			/* Code for ignore this packet. E.G LBMRD keepalive */
		} else {
			/* to do: Check for LBTRM or LBTRU */
			stats_nonlbmr_pktCount++;
			TS_printf(PRINT_WARNING, "\n UNKNOWN UDP packet found[%i]. stats_lbmr_pktCount unchanged: %i \n",stats_nonlbmr_pktCount, stats_lbmr_pktCount);
	  	}
		if (options.pcapFile) {
		  pcapSize = buildPcapRecord(&fromaddr, nBytes, &tv, AddrList);
		  if (pcapSize > 0) {
			nBytes = write(outFd, dataBuf, pcapSize);
			if (nBytes < 0) {
				perror("write");
				exit(errno);
			}
			if (nBytes != pcapSize) {
				fprintf(stderr, "Packet %d: Incomplete write - %d of %d written\n",
				        stats_pktCount, nBytes, pcapSize);
			}
		  }
		}

	} // while (1)
}

int
buildPcapRecord(struct sockaddr_in *sin, int recvSize, struct timeval *tv, TRaddr_t *traddr)
{
	int pktLength = recvSize;

	if (!options.raw) {
		/* Need to fill in fields in the UDP and IP header */
		pktLength = recvSize + sizeof(struct iphdr) + sizeof(struct udphdr);
		udpHdr->source = sin->sin_port;
		udpHdr->len = htons(recvSize + sizeof(struct udphdr));
		udpHdr->dest = traddr->port;
		ipHdr->saddr = sin->sin_addr.s_addr;
		ipHdr->tot_len = htons(recvSize + sizeof(struct iphdr) + sizeof(struct udphdr));
		ipHdr->daddr = traddr->addr;
		ipHdr->check = 0;
		ipHdr->check = cksum(sizeof(struct iphdr), (uint16_t *)ipHdr);
	}
	pcapHdr->ts_sec = tv->tv_sec;
	pcapHdr->ts_usec = tv->tv_usec;
	pcapHdr->incl_len = pcapHdr->orig_len = pktLength;
	return pktLength + sizeof(pcaprec_hdr_t);

}

/*
 * One-time init of the dummy IP and UDP headers
 */
void
initHeaders(void)
{
	memset(ipHdr, 0, sizeof(*ipHdr));
	memset(udpHdr, 0, sizeof(*udpHdr));

	/* Initialize the IP header */
	ipHdr->ihl = 5;
	ipHdr->version = 4;
	ipHdr->ttl = 16;
	ipHdr->protocol = IPPROTO_UDP;
	ipHdr->daddr = options.TR_group;

	udpHdr->dest = options.TR_port;

}

/*
 * One-time init of the PCAP file - write file header
 */
void
pcapInitFile(int fd)
{
	pcap_hdr_t *pHdr;
	pHdr = malloc(sizeof(pcap_hdr_t));
	pHdr->magic_number = PCAP_MAGIC;
	pHdr->version_major = PCAP_MAJOR;
	pHdr->version_minor = PCAP_MINOR;
	pHdr->thiszone = 0;
	pHdr->sigfigs = 0;
	pHdr->snaplen = MAX_RBUF;
	pHdr->network = PCAP_LINK_RAW;

	if (write(fd, pHdr, sizeof(pcap_hdr_t)) < 0) {
		perror("write(pcapHdr)");
		exit(errno);
	}
}

uint16_t
cksum(int count, uint16_t *addr)
{
	/* Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
	 */
	register long sum = 0;

	while( count > 1 )  {
		/*  This is the inner loop */
		sum += *addr++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if( count > 0 ) {
		sum += *(uint8_t *) addr;
	}

	/*  Fold 32-bit sum to 16 bits */
	while (sum>>16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (uint16_t) ~sum;
}

/* Utility to print the contents of a buffer in hex/ASCII format */
void TSdump(uint32_t level, const char *buffer, int size)
{
        int i,j;
        unsigned char c;
        char textver[20];

        if( (level & TS_current_log_level) == 0){
		return;
	}
	TS_printf(level, "\n");
        for (i=0;i<(size >> 4);i++) {
                for (j=0;j<16;j++) {
                        c = buffer[(i << 4)+j];
                        TS_printf(level, "%02x ",c);
                        textver[j] = ((c<0x20)||(c>0x7e))?'.':c;
                }
                textver[j] = 0;
                TS_printf(level, "\t%s\n",textver);
        }
        for (i=0;i<size%16;i++) {
                c = buffer[size-size%16+i];
                TS_printf(level, "%02x ",c);
                textver[i] = ((c<0x20)||(c>0x7e))?'.':c;
        }
        for (i=size%16;i<16;i++) {
                TS_printf(level, "   ");
                textver[i] = ' ';
        }
        textver[i] = 0;
        TS_printf(level, "\t%s\n",textver);
}


/* Keep local copy of the data */

int TS_assemble_udp_frags(uint16_t id, unsigned char * buff_ptr, int nBytes, int frag_offset, uint16_t  mf_flag, struct timeval tv, struct sockaddr_in *fromaddr){
	/* Assembles fragments and calls the decode function
	 * 	TO DO: Use function pointer to generalize routine
 	 * Returns 0 for success, or non-0 for failure.
 	 */
	struct frag_id *ip_frag_ptr;
	char * reassembled_msg_buff_ptr, *udp_ptr ;
	uint16_t msg_size;
	int val;
	struct frags_list * id_prev_list_ptr;
	struct frags_list * temp_ptr;

	TS_printf(PRINT_DEBUG|PRINT_IP_FRAG, "\n TS_assemble_udp_frags: (uint16_t id[%i], unsigned char * buff_ptr[%x], int nBytes[%i], int frag_offset[%i], uint16_t  mf_flag[%x],...)",  id,buff_ptr,nBytes,frag_offset,mf_flag);

 	/* Create the fragment */
	 ip_frag_ptr = malloc ( sizeof(struct frag_id) );
	 if (ip_frag_ptr == NULL ) { TS_printf(PRINT_ERROR, "\n MALLOC Error, quitting ... "); return -1 ; }
	 ip_frag_ptr->next = 0;

	 ip_frag_ptr->id = id;
	 ip_frag_ptr->frag_offset = frag_offset;
	 ip_frag_ptr->nBytes = nBytes;
	 ip_frag_ptr->data = malloc(nBytes);
	 if (ip_frag_ptr->data == 0 ){
		TS_printf(PRINT_ERROR, "MALLOC error in stats_lbmr_pktCount() ");
		return 1;
	 }
	 /* Copy the data */
	 memcpy(ip_frag_ptr->data, buff_ptr, nBytes);


	/* Add IP fragment to local list */
	id_prev_list_ptr = TS_add_ip_fragment (id, ip_frag_ptr, mf_flag );

	/* Try to reassemble the message if we previously received the last fragment */	
	if ((id_prev_list_ptr->next->got_last_frag != 0) && (id_prev_list_ptr->next->incomplete_flag == 0) ){

		TS_printf(PRINT_VERBOSE|PRINT_IP_FRAG, "\n TS_assemble_udp_frags: Last fragment flag %i of id %i, frag_offset = %i, nBytes =%i ", mf_flag, id, frag_offset, nBytes);
		msg_size = frag_offset+nBytes;


		/* reassemble the fragments */
		val = TS_reassmble_ip_fragment(id, id_prev_list_ptr, msg_size);
		if (val != 0){
			TS_printf(PRINT_ERROR, "\n ERROR! could not reassemble packet: val = %i", val);
			/* return 1; */
		} else {

			reassembled_msg_buff_ptr = id_prev_list_ptr->next->reassembled_msg;
	#if FRAG_DEBUG
			TS_printf(PRINT_DEBUG, "\n DEBUG: reassembled_msg_buff_ptr = %i", &id_prev_list_ptr->next->reassembled_msg);
			TS_printf(PRINT_DEBUG, "\n Reassembled packet :"); TSdump(PRINT_DEBUG, reassembled_msg_buff_ptr, msg_size);	
	#endif
			
			udp_ptr = (char *)(BUF_OFFSET(reassembled_msg_buff_ptr, sizeof(*udpHdr)));
			val = DecodeLbmrPacket((uint8_t *)udp_ptr, fromaddr, msg_size-sizeof(*udpHdr), &tv, AddrList);
			if ( val == 0) {
				stats_lbmr_pktCount++;
				TS_printf(PRINT_LBMR, "\n LBMR packet found. stats_lbmr_pktCount: %i", stats_lbmr_pktCount);
			} else {
				/* Check for LBTRM or LBTRU */
				TS_printf(PRINT_WARNING, "\n UNKNOWN UDP packet found val = %i. stats_lbmr_pktCount unchanged: %i",val,  stats_lbmr_pktCount);
			}
		}

		/* unlink the ID and free the structure */
		if( id_prev_list_ptr->next->next == 0){
			TS_printf(PRINT_IP_FRAG, "\n FRAG: free [%x] and setting to 0 ", id_prev_list_ptr->next);
			free(id_prev_list_ptr->next);
			id_prev_list_ptr->next = 0;
		} else {
			TS_printf(PRINT_IP_FRAG, "\n FRAG: free [%x] and setting to [%x] ", id_prev_list_ptr->next, id_prev_list_ptr->next->next);
			temp_ptr = id_prev_list_ptr->next;
			id_prev_list_ptr->next = id_prev_list_ptr->next->next;
			free(temp_ptr);
		}
		free(reassembled_msg_buff_ptr);
	}
 	return 0;
}/* TS_assemble_udp_frags */


int TS_reassmble_ip_fragment (uint16_t id, struct frags_list *prev_list, int msg_size){
  struct frags_list *list_iter;
  struct frag_id *frag_iter = 0;
  struct frag_id *prev_frag = 0;

  list_iter = prev_list->next;
  if( list_iter->id != id){
	TS_printf(PRINT_ERROR, "\n Error! TS_reassmble_ip_fragment ID : %i not found ");
	return 1;
  } else {
	if(list_iter->reassembled_msg==0){
		list_iter->reassembled_msg = malloc(msg_size);
		if (list_iter->reassembled_msg == NULL){
			TS_printf(PRINT_VERBOSE, "\n list_iter->reassembled_msg: malloc error ");
			return 2;
		}
	}
	frag_iter = list_iter->frag;
	prev_frag = frag_iter;
	 while ( frag_iter->next != 0 ){
		TS_printf(PRINT_DEBUG, "\n ID:%i copying dest %i, src %i, size %i",id,list_iter->reassembled_msg+frag_iter->frag_offset,frag_iter->data, frag_iter->nBytes);
	 	memcpy(list_iter->reassembled_msg+frag_iter->frag_offset, frag_iter->data, frag_iter->nBytes);
		frag_iter = frag_iter->next;
		free(prev_frag->data);
		free(prev_frag);
		prev_frag = frag_iter;
	 }
	 /* Have to deal with the last fragment  */
	 TS_printf(PRINT_DEBUG, "\n ID: copying dest %i, src %i, size %i",list_iter->reassembled_msg+frag_iter->frag_offset,frag_iter->data, frag_iter->nBytes);
	 memcpy(list_iter->reassembled_msg+frag_iter->frag_offset, frag_iter->data, frag_iter->nBytes);
	 free(frag_iter->data);
	 free(frag_iter);
  }
 return 0;
}/* End TS_reassmble_ip_fragment */


struct frags_list * TS_add_ip_fragment (uint16_t id, struct frag_id *ip_frag, uint16_t no_more_fragments ){
  /* Note that we return the list iterator just prior to the ID of interest */
  struct frags_list *list_iter, *new_list, *prev_list;
  struct frag_id *frag_iter;

  TS_printf(PRINT_IP_FRAG, "\n TS_add_ip_fragment ( id %i, struct frag_id %x, no_more_fragments %i)", id, &ip_frag, no_more_fragments);
  list_iter = &list_head;
  prev_list = list_iter;
  while ( list_iter->next != 0 && list_iter->id != id ){
	prev_list = list_iter;
 	list_iter = list_iter->next;
  }
  TS_printf(PRINT_IP_FRAG, "\n Head[%x] list_iter[%x] [ID %i]",&list_head, list_iter, list_iter->id);

  if( (list_iter->id == id) && (list_iter != &list_head)){ /* 4.6 .. ignore the head */
	frag_iter = list_iter->frag;
	/* Check if all the message was received if we had seen the last fragment */
	if( no_more_fragments == 0 ){
		list_iter->got_last_frag = 1;

		/* Check if you got the expected number of fragments */
		list_iter->incomplete_flag = list_iter->frags_received - (ip_frag->frag_offset / frag_iter->nBytes);
		TS_printf(PRINT_IP_FRAG, "\n Last fragment: list_iter->incomplete_flag (%i) = list_iter->frags_received (%i) - (ip_frag->frag_offset %i / frag_iter->nBytes %i)", list_iter->incomplete_flag, list_iter->frags_received, ip_frag->frag_offset, frag_iter->nBytes );
	} else {
		if( list_iter->incomplete_flag > 0 ){
			list_iter->incomplete_flag--;
		}
	}

	/* Add entry to the end of the linked list ID structure */
	while(frag_iter->next != 0){
		frag_iter = frag_iter->next;
	}
	 TS_printf(PRINT_IP_FRAG, "\n Adding entry to list, ID %i: ip_frag->offset %i, ip_frag->nBytes %i, frag_iter->frag_offset %i, frag_iter->nBytes = %i, ", id, ip_frag->frag_offset, ip_frag->nBytes, frag_iter->frag_offset, frag_iter->nBytes );
	 frag_iter->next = ip_frag;
	 list_iter->frags_received++;
	 return prev_list;
  } else {
	/* Start a new list with the ID */
	TS_printf(PRINT_IP_FRAG, "\n Starting new list, ID %i ", id);
	new_list = malloc (sizeof (struct frags_list));
	new_list->id = id;
	new_list->next = 0;	
	new_list->incomplete_flag = 0xFF;	
	new_list->frag = ip_frag;	
	new_list->frag->next = 0;	
	new_list->reassembled_msg = 0;
	new_list->got_last_frag = 0;
	new_list->frags_received = 1;
 	list_iter->next = new_list ;
  	return list_iter;
  }	
	
}/* TS_add_ip_fragment */





void TS_stats_OTID_print(void){
	TS_printf(PRINT_STATS,"\n TS_stats_OTID_print:");
	print_string_list(&TS_otids_list);	
}
void TS_stats_TQRS_print(void){
/* to do Then checkout lbmrcv, see if you can do the same thing, to determine if this was a remote TQR query. The DRO has the context ID embeedded in there.  */
	TS_printf(PRINT_STATS,"\n TS_stats_TQRS_print:");
	print_string_list(&TS_tqrs_list);	
}
void TS_stats_TMRS_print(void){
	TS_printf(PRINT_STATS,"\n TS_stats_TMRS_print:");
	print_string_list(&TS_tmrs_list);	
}
void TS_stats_WTQRS_print(void){
	TS_printf(PRINT_STATS,"\n TS_stats_WTQRS_print:");
	print_string_list(&TS_wtqrs_list);	
}
void TS_stats_DRO_print(void){
	TS_printf(PRINT_STATS,"\n TS_stats_DRO_print:");
	print_string_list(&TS_dros_list);	
}
void TS_stats_UMP_print(void){
	TS_printf(PRINT_STATS,"\n TS_stats_UMP_print:");
	print_string_list(&TS_umps_list);	
}
void TS_stats_ULB_print(void){
	TS_printf(PRINT_STATS,"\n TS_stats_ULB_print:");
	print_string_list(&TS_ulbs_list);	
}
	
void TS_print_stored_stats(void){
	struct timeval tv;
	char tmbuf[TS_DATESTR_SZ];

	gettimeofday(&tv, NULL);
	TS_tv2str(&tv,tmbuf);

	TS_printf(PRINT_STATS,"\n\n\t TS_print_stored_stats:%i.%i (%s)  ", tv.tv_sec, tv.tv_usec,tmbuf);
	TS_printf(PRINT_STATS,"\n ------------------------------------------------------------------------------------------------------------ \n");
    	TS_stats_ULB_print();
	TS_stats_DRO_print();
	TS_stats_UMP_print();
	TS_stats_TMRS_print();
	TS_stats_WTQRS_print();
	TS_stats_TQRS_print();
    	TS_stats_OTID_print();
        TS_printf(PRINT_STATS,"\n DETECTED STATS: lbmr_pktCount:%i, nonlbmr_pktCount:%i, otids:%i, tmrs:%i, tirs:%i, tqrs:%i, domain_stats:%i ulb_stats:%i dro_src:%i CtxAd:%i RD_KA:%i",
        stats_lbmr_pktCount, stats_nonlbmr_pktCount , stats_otids , stats_tmrs , stats_tirs , stats_tqrs , stats_domains, stats_ulbs, stats_dro_src, stats_ctxinfo, stats_lbmrd_keepalive);
	TS_printf(PRINT_STATS,"\n ------------------------------------------------------------------------------------------------------------ \n");
	if(TS_output_file != NULL){
		fflush(TS_output_file);
	}
}/* End TS_print_stored_stats() */
void TS_clear_stored_stats(void){
struct timeval tv;
	gettimeofday(&tv, NULL);
	TS_printf(PRINT_STATS,"\n\n    TS_clearing_stored_stats:%i.%i  ", tv.tv_sec, tv.tv_usec);
	clear_string_list(&TS_dros_list);
	clear_string_list(&TS_umps_list);
	clear_string_list(&TS_tmrs_list);
	clear_string_list(&TS_wtqrs_list);
	clear_string_list(&TS_tqrs_list);
	clear_string_list(&TS_otids_list);
	clear_string_list(&TS_ulbs_list);

	// Also clear accumulated stats_* ?
}/* End TS_clear_stored_stats() */

void TS_stats_OTID(char *tname, char * str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];

	TS_printf(PRINT_DEBUG, "\n TS_stats_OTID: otid[%s], str1[%s], tv[%i.%i], save_all_entries[%i]", tname, (str1!=NULL)?str1:" ", tv->tv_sec, tv->tv_usec, save_all_entries);
	
	
	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s",tmbuf, tname );
		res = strings_list_store(time_buf, str1, &TS_otids_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",%s,time:%s",str1, tmbuf );
		/* Store unique OTIDs */
		res = strings_list_store(tname, time_buf, &TS_otids_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n[%i.%i] New OTID Added %s %s", tv->tv_sec, tv->tv_usec, tname, (str1!=NULL)?       str1:" ");
	}
	
}/* End TS_stats_OTID() */

void TS_stats_TMR(char * tname, char * str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];
	
	TS_printf(PRINT_TMR, "\n TS_stats_TMR: tname[%s], str1[%s], tv[%i.%i], save_all_entries[%i]", tname, str1, tv->tv_sec, tv->tv_usec, save_all_entries);

	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s", tmbuf, tname);
		res = strings_list_store(time_buf, str1, &TS_tmrs_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",time:%s,%s", tmbuf, str1);
		res = strings_list_store(tname, time_buf, &TS_tmrs_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n[%i.%i] New TMR Stat Entry %s %s",tv->tv_sec, tv->tv_usec, tname,  (str1!=NULL)?       str1:" ");
	}
 }

void TS_stats_WTQR(char *tname,char * str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];

	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s", tmbuf, tname);
		res = strings_list_store(time_buf, str1, &TS_wtqrs_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",time:%s,%s", tmbuf, str1);
		res = strings_list_store(tname, time_buf, &TS_wtqrs_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n[%i.%i] New W-TQR Stat Entry %s %s", tv->tv_sec, tv->tv_usec, tname, (str1!=NULL)?       str1:" ");
	}
}

void TS_stats_TQR(char * tname, char *str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];
	
	TS_printf(PRINT_LBMR, "\n TS_stats_TQR: tname[%s], str1[%s], tv[%i.%i], save_all_entries[%i]", tname, str1, tv->tv_sec, tv->tv_usec, save_all_entries);

	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s", tmbuf, tname);
		res = strings_list_store(time_buf, str1, &TS_tqrs_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",time:%s,%s", tmbuf, str1);
		res = strings_list_store(tname, time_buf, &TS_tqrs_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n[%i.%i] New TQR Stat Entry %s,%s", tv->tv_sec, tv->tv_usec, tname,(str1!=NULL)?  str1:" ");
	}
}

void TS_stats_DRO(char * tname, char *str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];

	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s", tmbuf, tname);
		res = strings_list_store(time_buf, str1, &TS_dros_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",time:%s,%s", tmbuf, str1);
		res = strings_list_store(tname, time_buf, &TS_dros_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n New DRO Stat Entry %s,%s", tname, (str1!=NULL)? str1:" ");
	}
}

void TS_stats_UMP(char * tname, char *str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];

	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s", tmbuf, tname);
		res = strings_list_store(time_buf, str1, &TS_umps_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",time:%s,%s", tmbuf, str1);
		res = strings_list_store(tname, time_buf, &TS_umps_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n New UMP Stat Entry %s,%s", tname, (str1!=NULL)? str1:" ");
	}
}

void TS_stats_ULB(char * tname, char *str1, struct timeval *tv, char save_all_entries){
	char res, time_buf[TS_MAX_STRING_LENGTH];
	char tmbuf[TS_DATESTR_SZ];

	TS_tv2str(tv,tmbuf);
	if(save_all_entries == 1){
		snprintf(time_buf,TS_MAX_STRING_LENGTH,"time:%s,%s", tmbuf, tname);
		res = strings_list_store(time_buf, str1, &TS_ulbs_list);
	} else {
		snprintf(time_buf,TS_MAX_STRING_LENGTH,",time:%s,%s", tmbuf, str1);
		res = strings_list_store(tname, time_buf, &TS_ulbs_list);
	}
	if (res != 0){
		TS_printf(PRINT_VERBOSE,"\n New ULB Stat Entry %s,%s", tname, (str1!=NULL)? str1:" ");
	}
}

