/*
"umsub.c: application that receives messages from a set of topics
"  Streaming, Persistence, or Queuing (multiple receivers).

  Copyright (c) 2005-2012 Informatica Corporation  Permission is granted to licensees to use
  or alter this software for any purpose, including commercial applications,
  according to the terms laid out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
  LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF
  THE LIKELIHOOD OF SUCH DAMAGES.
*/

#ifdef __VOS__
#define _POSIX_C_SOURCE 200112L
#include <sys/time.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <sys/timeb.h>
	#define strcasecmp stricmp
	#define snprintf _snprintf
#else
	#include <unistd.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <signal.h>
	#include <sys/time.h>
	#include <pthread.h>
#endif
#if !defined(_WIN32)
	#include <sys/utsname.h>
#endif

#include "replgetopt.h"
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include "monmodopts.h"
#include "lbm-example-util.h"

#define UM_PUB_VERSION "0.1"

#if defined(_WIN32)
#   define SLEEP_SEC(x) Sleep((x)*1000)
#   define SLEEP_MSEC(x) Sleep(x)
#else
#   define SLEEP_SEC(x) sleep(x)
#   define SLEEP_MSEC(x) \
		do{ \
			if ((x) >= 1000){ \
				sleep((x) / 1000); \
				usleep((x) % 1000 * 1000); \
			} \
			else{ \
				usleep((x)*1000); \
			} \
		}while (0)
#endif /* _WIN32 */

const char Purpose[] = "Purpose: Receive messages on  multiple topics.";
const char Usage[] =
"Usage: %s [options]\n"
"  -A, --ascii-mode           Print message payload as ASCII text\n"
"  -B, --bufsize=#            Set receive socket buffer size to # (in MB)\n"
"  -c, --config=FILE          Use LBM configuration file FILE.\n"
"                             Multiple config files are allowed.\n"
"                             Example:  '-c file1.cfg -c file2.cfg'\n"
"  -C, --contexts=NUM         use NUM lbm_context_t objects\n"
"  -E, --exit                 exit and end upon receiving End-of-Stream notification\n"
"  -e, --end-flag=FILE        clean up and exit when file FILE is created\n"
"  -F, --hot-failover         Use hot failover receivers\n"
"  -h, --help                 display this help and exit\n"
"  -i, --initial-topic=NUM    use NUM as initial topic number\n"
"  -N, --channel              Use as initial channel number for Spectrum\n"
"  -o, --ouput=FILE           Dump metrics to CSV file\n"
"  -L, --linger=NUM           linger for NUM seconds after done\n"
"  -p, --print-metrics        Print metrics to stdout every N milliseconds\n"
"  -Q, --event-queue          Enable UM event queue\n"
"  -r, --root=STRING          use topic names with root of STRING\n"
"  -R, --receivers=NUM        create NUM receivers\n"
"  -s, --statistics           print statistics along with bandwidth\n"
"  -t, --measure-latency      Calculate latency based on message payload timestamp. Use twice for round trip latency\n"
"  -T, --eq-threads           Use N threads for event queue processing\n"
"  -v, --verbose              be verbose\n"
"  -w, --wildcard-pattern     Create topic as wildcard pattern (appends .*)\n"

MONOPTS_RECEIVER
MONMODULEOPTS_SENDER;

const char * OptionString = "AB:c:C:Ee:Fhi:N:o:L:p:Qr:R:stT:vw";
#define OPTION_MONITOR_RCV 0
#define OPTION_MONITOR_CTX 1
#define OPTION_MONITOR_TRANSPORT 2
#define OPTION_MONITOR_TRANSPORT_OPTS 3
#define OPTION_MONITOR_FORMAT 4
#define OPTION_MONITOR_FORMAT_OPTS 5
#define OPTION_MONITOR_APPID 6
#define DEFAULT_RESPONSE_LEN 25

typedef enum {
		RCV_READAY, 
		START_RCVS,
		NO_VALUE
	} ReqType;
	
const struct option OptionTable[] =
{
	{ "ascii", no_argument, NULL, 'A' },
	{ "bufsize", required_argument, NULL, 'B' },
	{ "config", required_argument, NULL, 'c' },
	{ "contexts", required_argument, NULL, 'C' },
	{ "help", no_argument, NULL, 'h' },
	{ "exit", no_argument, NULL, 'E'},
	{ "end-flag", required_argument, NULL, 'e' },
	{ "hot-failover", no_argument, NULL, 'F' },
	{ "initial-topic", required_argument, NULL, 'i' },
	{ "channel", required_argument, NULL, 'N' },
	{ "output-file", required_argument, NULL, 'o' },
	{ "linger", required_argument, NULL, 'L' },
	{ "print-metrics", required_argument, NULL, 'p' },
	{ "event-queue", no_argument, NULL, 'Q' },
	{ "root", required_argument, NULL, 'r' },
	{ "receivers", required_argument, NULL, 'R' },
	{ "statistics", no_argument, NULL, 's' },
	{ "measure-latency", no_argument, NULL, 't' },
	{ "eq-threads", required_argument, NULL, 'T' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "wildcard-pattern", no_argument, NULL, 'w' },
	{ "monitor-rcv", required_argument, NULL, OPTION_MONITOR_RCV },
	{ "monitor-ctx", required_argument, NULL, OPTION_MONITOR_CTX },
	{ "monitor-transport", required_argument, NULL, OPTION_MONITOR_TRANSPORT },
	{ "monitor-transport-opts", required_argument, NULL, OPTION_MONITOR_TRANSPORT_OPTS },
	{ "monitor-format", required_argument, NULL, OPTION_MONITOR_FORMAT },
	{ "monitor-format-opts", required_argument, NULL, OPTION_MONITOR_FORMAT_OPTS },
	{ "monitor-appid", required_argument, NULL, OPTION_MONITOR_APPID },
	{ NULL, 0, NULL, 0 }
};

#define DEFAULT_MAX_MESSAGES 10000000
#define MAX_NUM_RCVS 1000001
#define MAX_TOPIC_NAME_LEN 80
#define DEFAULT_NUM_RCVS 100
#define MAX_NUM_CTXS 50
#define MAX_OUTPUT_FILE_NAME_SIZE 256
#define DEFAULT_NUM_CTXS 1
#define DEFAULT_TOPIC_ROOT "29west.example.multi"
#define DEFAULT_INITIAL_TOPIC_NUMBER 0
#define DEFAULT_MAX_NUM_SRCS 10000
#define DEFAULT_NUM_SRCS 10
#define DEFAULT_LINGER_SECONDS 0

#define MAX_NUM_THREADS 16
int thrdidxs[MAX_NUM_THREADS];

struct Options {

	char application_id_string[1024];
	long bufsize;
	char *end_flg_file;
	int end_on_end;
	const lbmmon_format_func_t *format;
	const lbmmon_transport_func_t *transport;
	char format_options_string[1024];
	int initial_topic_number;
	int linger;
	int monitor_context;
	int monitor_context_ivl;
	int monitor_receiver;
	int monitor_receiver_ivl;
	int num_ctxs;
	int num_rcvs;
	int reserve_specific_index;
	int reserve_index;
	lbm_umq_index_info_t index;
	int pstats;
	char topicroot[80];
	char transport_options_string[1024];
	int verbose;
	int ascii;
	int use_hf;
	int channel;
	int eventq;
	int threads;
	int wildcard;
	int dump_to_file;
	char *output_file;
	FILE *ofp;
	int print_metrics;
	int measure_latency;
} options;

lbm_event_queue_t *evq = NULL;
int count = 0;
int msg_count = 0, total_msg_count = 0;
int byte_count = 0;
int unrec_count = 0, total_unrec_count = 0;
int close_recv = 0;
FILE *end_flg_fp = NULL;
int burst_loss = 0, total_burst_loss = 0;
/* Total stats */
int rxs = 0;
int otrs = 0;
int lstream = 0;
/* Previous Iteration */
int pre_rxs = 0;
int pre_otrs = 0;
int pre_lstream = 0;
lbm_ulong_t lost = 0, last_lost = 0;
lbm_rcv_transport_stats_t * stats = NULL;
int nstats = DEFAULT_NUM_SRCS;
lbm_response_t *response = NULL;
lbm_msg_t *response_msg = NULL;
ReqType isReady = NO_VALUE;

#if defined(_WIN32)
#define _SYS_NAMELEN    256
struct  utsname {
        char    sysname[_SYS_NAMELEN];  /* Name of OS */
        char    nodename[_SYS_NAMELEN]; /* Name of this network node */
        char    release[_SYS_NAMELEN];  /* Release level */
        char    version[_SYS_NAMELEN];  /* Version level */
        char    machine[_SYS_NAMELEN];  /* Hardware type */
};
#endif

void print_platform_info()
{
#if defined(__linux__) || defined(Darwin)
	struct utsname unm;

	if (uname(&unm) == 0) 
		printf("*      %s %s %s %s %s \n", unm.sysname, unm.nodename, unm.release, unm.version, unm.machine);
	else 
		printf("*      Could not determine system type\n");

#elif defined(_WIN32)
	SYSTEM_INFO sinfo;
	OSVERSIONINFO vinfo;
	struct utsname unm;
	DWORD namelen = sizeof(unm.nodename);

	GetSystemInfo(&sinfo);
  	vinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    	GetVersionEx(&vinfo);
	mul_snprintf(unm.sysname, sizeof(unm.sysname), "Windows");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
		mul_snprintf(unm.release, sizeof(unm.release), "Windows 95/98/Me (%d.%d)", vinfo.dwMajorVersion, vinfo.dwMinorVersion);
	} else if (vinfo.dwPlatformId = VER_PLATFORM_WIN32_NT) {
		mul_snprintf(unm.release, sizeof(unm.release), "Windows NT %d.%d", vinfo.dwMajorVersion, vinfo.dwMinorVersion);
	} else {
		mul_snprintf(unm.release, sizeof(unm.release), "Unknown(%d %d.%d)", vinfo.dwPlatformId, vinfo.dwMajorVersion, vinfo.dwMinorVersion);
	}
	mul_snprintf(unm.version, sizeof(unm.version), "Build %d %s", vinfo.dwBuildNumber, vinfo.szCSDVersion);
	switch (sinfo.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		mul_snprintf(unm.machine, sizeof(unm.machine), "x64-%x-%x %dx", sinfo.wProcessorLevel, sinfo.wProcessorRevision, sinfo.dwNumberOfProcessors);
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		mul_snprintf(unm.machine, sizeof(unm.machine), "IA64-%x-%x %dx", sinfo.wProcessorLevel, sinfo.wProcessorRevision, sinfo.dwNumberOfProcessors);
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		mul_snprintf(unm.machine, sizeof(unm.machine), "x86-%x-%x %dx", sinfo.wProcessorLevel, sinfo.wProcessorRevision, sinfo.dwNumberOfProcessors);
		break;
	default:
		mul_snprintf(unm.machine, sizeof(unm.machine), "%x-%x-%x %dx", sinfo.wProcessorArchitecture,
			sinfo.wProcessorLevel, sinfo.wProcessorRevision, sinfo.dwNumberOfProcessors);
		break;
	}
	GetComputerName(unm.nodename, &namelen);
	printf("*      %s %s %s %s %s\n", unm.sysname, unm.nodename, unm.release, unm.version, unm.machine);
#endif /* Linux */
}

double ow_total = 0.0, ow_min = 1000.0, ow_max = 0.0, ow_avg = 0.0;
int owts = 0;
char *rmessage;

/* Update 1-way latency stats */
void update_oneway_latency_stats(struct timeval *tsp, struct timeval *etv)
{
        double sec = 0.0;

        etv->tv_sec -= tsp->tv_sec;
        etv->tv_usec -= tsp->tv_usec;
        normalize_tv(etv);
        sec = (double)etv->tv_sec + (double)etv->tv_usec / 1000000.0;

	ow_total += sec;
	if (sec < ow_min) 
		ow_min = sec;
	if (sec >= ow_max) 
		ow_max = sec;
	owts++;
}

/*
 * For the elapsed time, calculate and print the msgs/sec and bits/sec as well
 * as any unrecoverable data.
 */
void print_bw(FILE *fp, struct timeval *tv, unsigned int msgs, unsigned int bytes, int unrec, lbm_ulong_t lost, int rxs, int otrs)
{
	char scale[] = {'\0', 'K', 'M', 'G'};
	int msg_scale_index = 0, bit_scale_index = 0, rps_scale_index = 0;
	double sec = 0.0, mps = 0.0, bps = 0.0, rps = 0.0;
	double kscale = 1000.0;
	
	if (tv->tv_sec == 0 && tv->tv_usec == 0) return;/* avoid div by 0 */
	sec = (double)tv->tv_sec + (double)tv->tv_usec / 1000000.0;
	mps = (double)msgs/sec;
	rps = (double)rxs/sec;
	bps = (double)bytes*8/sec;
	
	while (mps >= kscale) {
		mps /= kscale;
		msg_scale_index++;
	}

	while (rps >= kscale) {
		rps /= kscale;
		rps_scale_index++;
	}
	
	while (bps >= kscale) {
		bps /= kscale;
		bit_scale_index++;
	}

	fprintf(fp, "%-5.4g secs.  %-5.4g %cmsgs/sec.  %-5.4g %cbps [Total: %d]", sec, mps, scale[msg_scale_index], bps, scale[bit_scale_index], total_msg_count);
	
	if (lost != 0 || unrec != 0 || burst_loss != 0) {
		fprintf(fp, " [%lu pkts lost, %u msgs unrecovered, %d loss bursts]", lost, unrec, burst_loss);
		burst_loss = 0;
	}

	fputs("\n",fp);
	fflush(fp);
}

/* Callback function used for printing metrics to stdout and dumping to file if configured */
void print_metrics(lbm_context_t *ctx, const void *clientd)
{
        struct Options *opts = &options;

        printf("Messages Received: Live[%d] RX[%d] OTR[%d]", lstream - pre_lstream, rxs - pre_rxs, otrs - pre_otrs);
	if (opts->measure_latency > 0)
	{
		double ow_avg = ow_total / owts;
		printf(" Latency Min[%.06f] Max[%.06f] Avg[%.06f]", ow_min, ow_max, ow_avg);
	}
	printf("\n");

	if (opts->dump_to_file)
	{
		fprintf(opts->ofp, "%d,%d,%d,%.04f,%.04f,%.04f\n", lstream - pre_lstream, rxs - pre_rxs, otrs - pre_otrs, ow_min, ow_max, ow_avg);
		fflush(opts->ofp);
	}
}

/* Utility to print the contents of a buffer in hex/ASCII format */
void dump(const char *buffer, int size)
{
        int i,j;
        unsigned char c;
        char textver[20];

        for (i=0;i<(size >> 4);i++) {
                for (j=0;j<16;j++) {
                        c = buffer[(i << 4)+j];
                        printf("%02x ",c);
                        textver[j] = ((c<0x20)||(c>0x7e))?'.':c;
                }
                textver[j] = 0;
                printf("\t%s\n",textver);
        }
        for (i=0;i<size%16;i++) {
                c = buffer[size-size%16+i];
                printf("%02x ",c);
                textver[i] = ((c<0x20)||(c>0x7e))?'.':c;
        }
        for (i=size%16;i<16;i++) {
                printf("   ");
                textver[i] = ' ';
        }
        textver[i] = 0;
        printf("\t%s\n",textver);
}

/* Print transport statistics */
void print_stats(FILE *fp, lbm_rcv_transport_stats_t stats)
{
	switch (stats.type) {
	case LBM_TRANSPORT_STAT_TCP:
		fprintf(fp, " [%s], received %lu, LBM %lu/%lu/%lu\n",
				stats.source,stats.transport.tcp.bytes_rcved,
				stats.transport.tcp.lbm_msgs_rcved,
				stats.transport.tcp.lbm_msgs_no_topic_rcved,
				stats.transport.tcp.lbm_reqs_rcved);
		break;
	case LBM_TRANSPORT_STAT_LBTRM:
		{
			char stmstr[256] = "", txstr[256] = "";

			if (stats.transport.lbtrm.nak_tx_max > 0) {
				/* we usually don't use sprintf, but should be OK here for the moment. */
				sprintf(stmstr, ", nak stm %lu/%lu/%lu",
						stats.transport.lbtrm.nak_stm_min, stats.transport.lbtrm.nak_stm_mean,
						stats.transport.lbtrm.nak_stm_max);
				sprintf(txstr, ", nak tx %lu/%lu/%lu",
						stats.transport.lbtrm.nak_tx_min, stats.transport.lbtrm.nak_tx_mean,
						stats.transport.lbtrm.nak_tx_max);
			}
			fprintf(fp, " [%s], received %lu/%lu, dups %lu, loss %lu, naks %lu/%lu, ncfs %lu-%lu-%lu-%lu, unrec %lu/%lu%s%s\n",
					stats.source,
					stats.transport.lbtrm.msgs_rcved, stats.transport.lbtrm.bytes_rcved,
					stats.transport.lbtrm.duplicate_data,
					stats.transport.lbtrm.lost,
					stats.transport.lbtrm.naks_sent, stats.transport.lbtrm.nak_pckts_sent,
					stats.transport.lbtrm.ncfs_ignored, stats.transport.lbtrm.ncfs_shed,
					stats.transport.lbtrm.ncfs_rx_delay, stats.transport.lbtrm.ncfs_unknown,
					stats.transport.lbtrm.unrecovered_txw,
					stats.transport.lbtrm.unrecovered_tmo,
					stmstr, txstr);
		}
		break;
	case LBM_TRANSPORT_STAT_LBTRU:
		{
			char stmstr[256] = "", txstr[256] = "";

			if (stats.transport.lbtru.nak_tx_max > 0) {
				/* we usually don't use sprintf, but should be OK here for the moment. */
				sprintf(stmstr, ", nak stm %lu/%lu/%lu",
						stats.transport.lbtru.nak_stm_min, stats.transport.lbtru.nak_stm_mean,
						stats.transport.lbtru.nak_stm_max);
				sprintf(txstr, ", nak tx %lu/%lu/%lu",
						stats.transport.lbtru.nak_tx_min, stats.transport.lbtru.nak_tx_mean,
						stats.transport.lbtru.nak_tx_max);
			}
			fprintf(fp, " [%s], LBM %lu/%lu/%lu, received %lu/%lu, dups %lu, loss %lu, naks %lu/%lu, ncfs %lu-%lu-%lu-%lu, unrec %lu/%lu%s%s\n",
					stats.source,
					stats.transport.lbtru.lbm_msgs_rcved,
					stats.transport.lbtru.lbm_msgs_no_topic_rcved,
					stats.transport.lbtru.lbm_reqs_rcved,
					stats.transport.lbtru.msgs_rcved, stats.transport.lbtru.bytes_rcved,
					stats.transport.lbtru.duplicate_data,
					stats.transport.lbtru.lost,
					stats.transport.lbtru.naks_sent, stats.transport.lbtru.nak_pckts_sent,
					stats.transport.lbtru.ncfs_ignored, stats.transport.lbtru.ncfs_shed,
					stats.transport.lbtru.ncfs_rx_delay, stats.transport.lbtru.ncfs_unknown,
					stats.transport.lbtru.unrecovered_txw,
					stats.transport.lbtru.unrecovered_tmo,
					stmstr, txstr);
		}
		break;
	case LBM_TRANSPORT_STAT_LBTIPC:
		{
			fprintf(fp, " [%s] Received %lu msgs/%lu bytes. "
					"%lu LBM msgs, %lu no topics, %lu requests.\n",
					stats.source,
					stats.transport.lbtipc.msgs_rcved,
					stats.transport.lbtipc.bytes_rcved,
					stats.transport.lbtipc.lbm_msgs_rcved,
					stats.transport.lbtipc.lbm_msgs_no_topic_rcved,
					stats.transport.lbtipc.lbm_reqs_rcved);
		}
		break;
	case LBM_TRANSPORT_STAT_LBTRDMA:
		{
			fprintf(fp, " [%s] Received %lu msgs/%lu bytes. "
					"%lu LBM msgs, %lu no topics, %lu requests.\n",
					stats.source,
					stats.transport.lbtrdma.msgs_rcved,
					stats.transport.lbtrdma.bytes_rcved,
					stats.transport.lbtrdma.lbm_msgs_rcved,
					stats.transport.lbtrdma.lbm_msgs_no_topic_rcved,
					stats.transport.lbtrdma.lbm_reqs_rcved);
		}
		break;
	default:
		break;
	}
	fflush(fp);
}

/* Event queue monitor callback (passed into lbm_event_queue_create()) */
int evq_monitor(lbm_event_queue_t *evq, int event, size_t evq_size,
                                lbm_ulong_t event_delay_usec, void *clientd)
{
        printf("event queue threshold exceeded - event %x, sz %lu, delay %lu\n",
                   event, evq_size, event_delay_usec);
        return 0;
}

/* context event handler for UMQ events */
int handle_ctx_event(lbm_context_t *ctx, int event, void *ed, void *cd)
{
        switch (event) {
        case LBM_CONTEXT_EVENT_UMQ_REGISTRATION_ERROR:
                {
                        const char *errstr = (const char *)ed;

                        printf("Error registering ctx with UMQ queue: %s\n", errstr);
                }
                break;
        case LBM_CONTEXT_EVENT_UMQ_REGISTRATION_SUCCESS_EX:
                {
                        lbm_context_event_umq_registration_ex_t *reg = (lbm_context_event_umq_registration_ex_t *)ed;

                        printf("UMQ queue \"%s\"[%x][%s][%u] ctx registration. ID %" PRIx64 " Flags %x ", reg->queue, reg->queue_id, reg->queue_instance, reg->queue_instance_index,
                                reg->registration_id, reg->flags);
                        if (reg->flags & LBM_CONTEXT_EVENT_UMQ_REGISTRATION_COMPLETE_EX_FLAG_QUORUM)
                                printf("QUORUM ");
                        printf("\n");
                }
                break;
        case LBM_CONTEXT_EVENT_UMQ_REGISTRATION_COMPLETE_EX:
                {
                        lbm_context_event_umq_registration_complete_ex_t *reg = (lbm_context_event_umq_registration_complete_ex_t *)ed;

                        printf("UMQ queue \"%s\"[%x] ctx registration complete. ID %" PRIx64 " Flags %x ", reg->queue, reg->queue_id, reg->registration_id, reg->flags);
                        if (reg->flags & LBM_CONTEXT_EVENT_UMQ_REGISTRATION_COMPLETE_EX_FLAG_QUORUM)
                                printf("QUORUM ");
                        printf("\n");
                }
                break;
        case LBM_CONTEXT_EVENT_UMQ_INSTANCE_LIST_NOTIFICATION:
                {
                        const char *evstr = (const char *)ed;

                        printf("UMQ IL Notification: %s\n", evstr);
                }
                break;
        default:
                printf("Unknown context event %d\n", event);
                break;
        }
        return 0;
}

/* Callback received message handler (passed into lbm_rcv_create()) */
int rcv_handle_msg(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
	struct Options *opts = &options;
	char indexstr[LBM_UMQ_MAX_INDEX_LEN + 1] = "";
	char response_msg[DEFAULT_RESPONSE_LEN] = {'\0'};
	
	switch (msg->type) {
	case LBM_MSG_DATA:
		/*
		 * Data message received.
		 * All we do is increment the counters.
		 * We want to display aggregate reception rates for all
		 * receivers.
		 */
		msg_count++;
		total_msg_count++;
		byte_count += msg->len;

		if (opts->ascii) {
                        int n = msg->len;
                        const char *p = msg->data;
                        while (n--)
                        {
                                putchar(*p++);
                        }
                        if (opts->ascii > 1) putchar('\n');
                }
	
		if (opts->verbose) {
			printf("[@%d.%06d]", (int)msg->tsp.tv_sec, (int)msg->tsp.tv_usec);
                        if(msg->channel_info != NULL) {
                                printf("[%s:%u][%s][%u]%s%s%s%s, %lu bytes\n",
                                        msg->topic_name, msg->channel_info->channel_number,
                                        msg->source, msg->sequence_number,
                                        ((msg->flags & LBM_MSG_FLAG_RETRANSMIT) ? "-RX-" : ""),
                                        ((msg->flags & LBM_MSG_FLAG_HF_DUPLICATE) ? "-HFDUP-" : ""),
                                        ((msg->flags & LBM_MSG_FLAG_HF_PASS_THROUGH) ? "-PASS-" : ""),
                                        ((msg->flags & LBM_MSG_FLAG_OTR) ? "-OTR-" : ""),
                                        msg->len);
                        } else {
                                printf("[%s][%s][%u]%s%s%s%s, %lu bytes\n",
                                        msg->topic_name, msg->source, msg->sequence_number,
                                        ((msg->flags & LBM_MSG_FLAG_RETRANSMIT) ? "-RX-" : ""),
                                        ((msg->flags & LBM_MSG_FLAG_HF_DUPLICATE) ? "-HFDUP-" : ""),
                                        ((msg->flags & LBM_MSG_FLAG_HF_PASS_THROUGH) ? "-PASS-" : ""),
                                        ((msg->flags & LBM_MSG_FLAG_OTR) ? "-OTR-" : ""),
                                        msg->len);
                        }

                        if (opts->verbose > 1)
                                dump(msg->data, msg->len);
		}

		if (opts->measure_latency > 0)
		{
			struct timeval msgstarttv, msgendtv;

			/* Update 1-way latency metrics */
			current_tv(&msgendtv);
                        memcpy(&msgstarttv, msg->data, sizeof(msgstarttv));
			update_oneway_latency_stats(&msgstarttv, &msgendtv);
		}
		if (opts->measure_latency > 1)
		{
			/* Return source pointer should be in the clientd */
			lbm_src_t *rsrc = (lbm_src_t *) clientd;

			/* Send timestamp back to publisher to calculate round trip time */
			memcpy(rmessage, msg->data, msg->len);
			if (lbm_src_send(rsrc, rmessage, msg->len, LBM_MSG_FLUSH | LBM_SRC_NONBLOCK) == LBM_FAILURE) {
                                fprintf(stderr, "lbm_src_send: %s. Not all return messages will make it back.\n", lbm_errmsg());
                        }
		}

		/* Global app level stats */
		if(msg->flags & LBM_MSG_FLAG_RETRANSMIT)
            		rxs++;
		else if(msg->flags & LBM_MSG_FLAG_OTR)
            		otrs++;
        	else
            		lstream++;
		break;
	case LBM_MSG_BOS:
			printf("[%s][%s], Beginning of Transport Session\n", msg->topic_name, msg->source);
		break;
	case LBM_MSG_EOS:
			printf("[%s][%s], End of Transport Session\n", msg->topic_name, msg->source);
		if (opts->end_on_end)
			close_recv = 1;
		break;
	case LBM_MSG_NO_SOURCE_NOTIFICATION:
		if (opts->verbose)
			printf("[%s], no sources found for topic\n", msg->topic_name);
		break;
	case LBM_MSG_UNRECOVERABLE_LOSS:
		unrec_count++;
		total_unrec_count++;
		if (opts->verbose) {
			printf("[%s][%s][%u], LOST\n",
				   msg->topic_name, msg->source, msg->sequence_number);
		}
		break;
	case LBM_MSG_UNRECOVERABLE_LOSS_BURST:
		burst_loss++;
		total_burst_loss++;
		if (opts->verbose) {
			printf("[%s][%s][%u], LOST BURST\n",
				   msg->topic_name, msg->source, msg->sequence_number);
		}
		break;
	case LBM_MSG_REQUEST:
		/*
		 * Request message received.
		 * Just increment counters. We don't bother with responses here.
		 */
		msg_count++;
		total_msg_count++;
		byte_count += msg->len;
		response = NULL;
		if (opts->verbose) {
			printf("[%s][%s][%u], Request\n",
				   msg->topic_name, msg->source, msg->sequence_number);
		}
		
		printf("[%s][%s][%u] [%s], Request\n",
				   msg->topic_name, msg->source, msg->sequence_number, msg->data);
		lbm_msg_retain(msg);
		if (strcmp (msg->data, "IS_RCV_READAY") == 0)
		{
			printf("Setting isReady = RCV_READAY\n");
			isReady = RCV_READAY;
		}
		else if (strcmp (msg->data, "START_RCVS") == 0)
		{
			printf("Setting isReady = START_RCVS\n");
			isReady = START_RCVS;
		}
		response = msg->response;
		break;
	case LBM_MSG_UME_REGISTRATION_ERROR:
                printf("[%s][%s] UME registration error: %s\n", msg->topic_name, msg->source, msg->data);
                break;
        case LBM_MSG_UME_REGISTRATION_SUCCESS:
                {
                        lbm_msg_ume_registration_t *reg = (lbm_msg_ume_registration_t *)(msg->data);

                        printf("[%s][%s] UME registration successful. SrcRegID %u RcvRegID %u\n",
                                msg->topic_name, msg->source, reg->src_registration_id, reg->rcv_registration_id);
                }
                break;
        case LBM_MSG_UME_REGISTRATION_SUCCESS_EX:
                {
                        lbm_msg_ume_registration_ex_t *reg = (lbm_msg_ume_registration_ex_t *)(msg->data);

                        printf("[%s][%s] store %u: %s UME registration successful. SrcRegID %u RcvRegID %u. Flags %x ",
                                msg->topic_name, msg->source, reg->store_index, reg->store,
                                reg->src_registration_id, reg->rcv_registration_id, reg->flags);
                        if (reg->flags & LBM_MSG_UME_REGISTRATION_SUCCESS_EX_FLAG_OLD)
                                printf("OLD[SQN %x] ", reg->sequence_number);
                        if (reg->flags & LBM_MSG_UME_REGISTRATION_SUCCESS_EX_FLAG_NOCACHE)
                                printf("NOCACHE ");
                        if (reg->flags & LBM_MSG_UME_REGISTRATION_SUCCESS_EX_FLAG_RPP)
                                printf("RPP ");
                        printf("\n");
                }
                break;
	case LBM_MSG_UME_REGISTRATION_COMPLETE_EX:
                {
                        lbm_msg_ume_registration_complete_ex_t *reg = (lbm_msg_ume_registration_complete_ex_t *)(msg->data);

                        printf("[%s][%s] UME registration complete. SQN %x. Flags %x ",
                                msg->topic_name, msg->source, reg->sequence_number, reg->flags);
                        if (reg->flags & LBM_MSG_UME_REGISTRATION_COMPLETE_EX_FLAG_QUORUM)
                                printf("QUORUM ");
                        if (reg->flags & LBM_MSG_UME_REGISTRATION_COMPLETE_EX_FLAG_RXREQMAX)
                                printf("RXREQMAX ");
                        printf("\n");
                }
                break;
        case LBM_MSG_UME_DEREGISTRATION_SUCCESS_EX:
                {
                        lbm_msg_ume_deregistration_ex_t *dereg = (lbm_msg_ume_deregistration_ex_t *)(msg->data);

                        printf("[%s][%s] store %u: %s UME deregistration successful. SrcRegID %u RcvRegID %u. Flags %x ",
                                        msg->topic_name, msg->source, dereg->store_index, dereg->store,
                                        dereg->src_registration_id, dereg->rcv_registration_id, dereg->flags);
                        if (dereg->flags & LBM_MSG_UME_REGISTRATION_SUCCESS_EX_FLAG_OLD)
                                printf("OLD[SQN %x] ", dereg->sequence_number);
                        if (dereg->flags & LBM_MSG_UME_REGISTRATION_SUCCESS_EX_FLAG_NOCACHE)
                                printf("NOCACHE ");
                        if (dereg->flags & LBM_MSG_UME_REGISTRATION_SUCCESS_EX_FLAG_RPP)
                                printf("RPP ");
                        printf("\n");
                }
                break;
         case LBM_MSG_UME_DEREGISTRATION_COMPLETE_EX:
                {
                      printf("[%s][%s] UME deregistration complete.\n", msg->topic_name, msg->source);
                }
                break;
        case LBM_MSG_UME_REGISTRATION_CHANGE:
                printf("[%s][%s] UME registration change: %s\n", msg->topic_name, msg->source, msg->data);
                break;
	case LBM_MSG_UMQ_REGISTRATION_COMPLETE_EX:
                {
                        lbm_msg_umq_registration_complete_ex_t *reg = (lbm_msg_umq_registration_complete_ex_t *)(msg->data);
                        const char *type = "UMQ";

                        if (reg->flags & LBM_MSG_UMQ_REGISTRATION_COMPLETE_EX_FLAG_ULB)
                                type = "ULB";

                        printf("[%s][%s] %s \"%s\"[%x] registration complete. AssignID %x. Flags %x ", msg->topic_name, msg->source, type, reg->queue, reg->queue_id,
                                reg->assignment_id, reg->flags);
                        if (reg->flags & LBM_MSG_UMQ_REGISTRATION_COMPLETE_EX_FLAG_QUORUM)
                                printf("QUORUM ");
                        printf("\n");
                        if (opts->reserve_index) {
                                if (lbm_rcv_umq_index_reserve(rcv, NULL, opts->reserve_specific_index ? &(opts->index) : NULL) == LBM_FAILURE) {
                                        fprintf(stderr, "lbm_rcv_umq_index_reserve: %s\n", lbm_errmsg());
                                        exit(1);
                                }
                        }
                }
                break;
        case LBM_MSG_UMQ_DEREGISTRATION_COMPLETE_EX:
                {
                        lbm_msg_umq_deregistration_complete_ex_t *reg = (lbm_msg_umq_deregistration_complete_ex_t *)(msg->data);
                        const char *type = "UMQ";

                        if (reg->flags & LBM_MSG_UMQ_DEREGISTRATION_COMPLETE_EX_FLAG_ULB)
                                type = "ULB";

                        printf("[%s][%s] %s \"%s\"[%x] deregistration complete. Flags %x ", msg->topic_name, msg->source, type, reg->queue, reg->queue_id, reg->flags);
                        printf("\n");
                        close_recv = 1;
                }
                break;
        case LBM_MSG_UMQ_INDEX_ASSIGNMENT_ELIGIBILITY_ERROR:
                printf("[%s][%s] UMQ index assignment eligibility error: %s\n", msg->topic_name, msg->source, msg->data);
                break;
        case LBM_MSG_UMQ_INDEX_ASSIGNMENT_ERROR:
                printf("[%s][%s] UMQ index assignment error: %s\n", msg->topic_name, msg->source, msg->data);
                break;
        case LBM_MSG_UMQ_INDEX_ASSIGNMENT_ELIGIBILITY_START_COMPLETE_EX:
                {
                        lbm_msg_umq_index_assignment_eligibility_start_complete_ex_t *ias = (lbm_msg_umq_index_assignment_eligibility_start_complete_ex_t *)(msg->data);
                        const char *type = "UMQ";

                        if (ias->flags & LBM_MSG_UMQ_INDEX_ASSIGNMENT_ELIGIBILITY_START_COMPLETE_EX_FLAG_ULB)
                                type = "ULB";

                        printf("[%s][%s] %s \"%s\"[%x] index assignment eligibility start complete. Flags %x\n", msg->topic_name, msg->source, type, ias->queue, ias->queue_id, ias->flags);
                }
                break;
	case LBM_MSG_UMQ_INDEX_ASSIGNMENT_ELIGIBILITY_STOP_COMPLETE_EX:
                {
                        lbm_msg_umq_index_assignment_eligibility_stop_complete_ex_t *ias = (lbm_msg_umq_index_assignment_eligibility_stop_complete_ex_t *)(msg->data);
                        const char *type = "UMQ";

                        if (ias->flags & LBM_MSG_UMQ_INDEX_ASSIGNMENT_ELIGIBILITY_STOP_COMPLETE_EX_FLAG_ULB)
                                type = "ULB";

                        printf("[%s][%s] %s \"%s\"[%x] index assignment eligibility stop complete. Flags %x\n", msg->topic_name, msg->source, type, ias->queue, ias->queue_id, ias->flags);
                }
                break;
        case LBM_MSG_UMQ_INDEX_ASSIGNED_EX:
                {
                        lbm_msg_umq_index_assigned_ex_t *ia = (lbm_msg_umq_index_assigned_ex_t *)(msg->data);
                        const char *type = "UMQ";

                        if (ia->flags & LBM_MSG_UMQ_INDEX_ASSIGNED_EX_FLAG_ULB)
                                type = "ULB";

                        if (ia->index_info.flags & LBM_UMQ_INDEX_FLAG_NUMERIC)
                                snprintf(indexstr, sizeof(indexstr), "%" PRIu64, *(lbm_uint64_t *)(&(ia->index_info.index)));
                        else
                                snprintf(indexstr, sizeof(indexstr), "\"%s\"", ia->index_info.index);

                        printf("[%s][%s] %s \"%s\"[%x] beginning of index assignment for index %s. Flags %x\n", msg->topic_name, msg->source, type, ia->queue, ia->queue_id, indexstr, ia->flags);
                }
                break;
        case LBM_MSG_UMQ_INDEX_RELEASED_EX:
                {
                        lbm_msg_umq_index_released_ex_t *ir = (lbm_msg_umq_index_released_ex_t *)(msg->data);
                        const char *type = "UMQ";

                        if (ir->flags & LBM_MSG_UMQ_INDEX_RELEASED_EX_FLAG_ULB)
                                type = "ULB";

                        if (ir->index_info.flags & LBM_UMQ_INDEX_FLAG_NUMERIC)
                                snprintf(indexstr, sizeof(indexstr), "%" PRIu64, *(lbm_uint64_t *)(&(ir->index_info.index)));
                        else
                                snprintf(indexstr, sizeof(indexstr), "\"%s\"", ir->index_info.index);

                        printf("[%s][%s] %s \"%s\"[%x] end of index assignment for index %s. Flags %x\n", msg->topic_name, msg->source, type, ir->queue, ir->queue_id, indexstr, ir->flags);

                }
                break;
	default:
		printf("Unknown lbm_msg_t type %x [%s][%s]\n", msg->type, msg->topic_name, msg->source);
		break;
	}
	/* LBM automatically deletes the lbm_msg_t object unless we retain it. */
	return 0;
}

#if !defined(_WIN32)
static int LossRate = 0;

static
void
SigHupHandler(int signo)
{
	if (LossRate >= 100)
	{
		return;
	}
	LossRate += 5;
	if (LossRate > 100)
	{
		LossRate = 100;
	}
	lbm_set_lbtrm_loss_rate(LossRate);
	lbm_set_lbtru_loss_rate(LossRate);
}

static
void
SigUsr1Handler(int signo)
{
	if (LossRate >= 100)
	{
		return;
	}
	LossRate += 10;
	if (LossRate > 100)
	{
		LossRate = 100;
	}
	lbm_set_lbtrm_loss_rate(LossRate);
	lbm_set_lbtru_loss_rate(LossRate);
}

static
void
SigUsr2Handler(int signo)
{
	LossRate = 0;
	lbm_set_lbtrm_loss_rate(LossRate);
	lbm_set_lbtru_loss_rate(LossRate);
}

void  INThandler(int sig)
{
     	signal(sig, SIG_IGN);
     	printf("Process Interrupted!\n");
      	printf("Quitting.... received %u messages\n", total_msg_count);
	close_recv = 1;
	SLEEP_SEC(2);
     	exit(0);
}

#endif

void process_cmdline(int argc, char **argv) {

	struct Options *opts = &options;
	int c, errflag = 0, i;

	/* Print header */
	printf("*********************************************************************\n");
	printf("*\n");
	printf("*      UMTools\n");
	printf("*      umsub - Version %s\n", UM_PUB_VERSION);
	printf("*      %s\n", lbm_version());
	print_platform_info();
	printf("*\n");
	printf("*      Parameters: ");
	for (i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n*\n");
	printf("*********************************************************************\n");

	
	/* Set default values */
	memset(opts, 0, sizeof(*opts));
	opts->bufsize = 8;
	opts->end_flg_file = NULL;
	opts->format = lbmmon_format_csv_module();
	opts->initial_topic_number = DEFAULT_INITIAL_TOPIC_NUMBER;
	opts->linger = DEFAULT_LINGER_SECONDS;
	opts->num_ctxs = DEFAULT_NUM_CTXS;
	opts->num_rcvs = DEFAULT_NUM_RCVS;
	strncpy(opts->topicroot, DEFAULT_TOPIC_ROOT, sizeof(opts->topicroot));
	opts->transport = lbmmon_transport_lbm_module();

	while ((c = getopt_long(argc, argv, OptionString, OptionTable, NULL)) != EOF)
	{
		switch (c)
		{
			case 'A':
				opts->ascii++;
				break;
			case 'B':
				opts->bufsize = atoi(optarg);
				break;
			case 'c':
				/* Initialize configuration parameters from a file. */
				if (lbm_config(optarg) == LBM_FAILURE) {
					fprintf(stderr, "lbm_config: %s\n", lbm_errmsg());
					exit(1);
				}
				break;
			case 'C':
				opts->num_ctxs = atoi(optarg);
				if (opts->num_ctxs > MAX_NUM_CTXS)
				{
					fprintf(stderr, "Too many contexts specified. "
									"Max number of contexts is %d\n", MAX_NUM_CTXS);
					errflag++;
				}
				break;
			case 'E':
				opts->end_on_end = 1;
				break;
			case 'e':
				opts->end_flg_file = optarg;
				break;
			case 'F':
				opts->use_hf++;
				break;
			case 'i':
				opts->initial_topic_number = atoi(optarg);
				break;
			case 'L':
				opts->linger = atoi(optarg);
				break;		
			case 'o':
				opts->dump_to_file++;
				//opts->output_file = malloc(MAX_OUTPUT_FILE_NAME_SIZE);
				//strncpy(opts->output_file, optarg, sizeof(MAX_OUTPUT_FILE_NAME_SIZE));
				opts->output_file = optarg;
				if ( (opts->ofp = fopen(opts->output_file, "w")) == NULL)
				{
					fprintf(stderr, "Error opening output file");
					exit(1);
				}
				fprintf(opts->ofp, "Messages,RX,OTR,Latency Min,Latency Max,Latency Avg\n");
				break;
			case 'h':
				fprintf(stderr, "%s\n", Purpose);
				fprintf(stderr, Usage, argv[0]);
				exit(0);
			case 'N':
				opts->channel = atoi(optarg);
				break;
			case 'p':
				opts->print_metrics = atoi(optarg);
				break;
			case 'r':
				strncpy(opts->topicroot, optarg, sizeof(opts->topicroot));
				break;
			case 'Q':
				opts->eventq++;
				break;
			case 'R':
				opts->num_rcvs = atoi(optarg);
				if (opts->num_rcvs > MAX_NUM_RCVS)
				{
					fprintf(stderr, "Too many receivers specified. "
									"Max number of receivers is %d\n", MAX_NUM_RCVS);
					errflag++;
				}
				break;
			case 's':
				opts->pstats++;
				break;
			case 't':
				opts->measure_latency++;
				break;
			case 'T':
                                opts->threads = atoi(optarg);
                                if (opts->threads > MAX_NUM_THREADS)
                                {
                                        fprintf(stderr, "Too many threads specified. Max number of threads is %d.\n",MAX_NUM_THREADS);
                                        errflag++;
                                }
                                break;
			case 'v':
				opts->verbose++;
				break;
			case 'w':
				opts->wildcard++;
				break;
			case 'X':
                        {
                                if (optarg != NULL) {
                                        int sscanf_res = 0;
                                        sscanf_res = sscanf(optarg, "%" SCNu64, (long unsigned int*)&(opts->index.index));
                                        if (sscanf_res == 1) {
                                                /* Assume numeric index. */
                                                opts->index.index_len = sizeof(lbm_uint64_t);
                                                opts->index.flags |= LBM_UMQ_INDEX_FLAG_NUMERIC;
                                        }
                                        else {
                                                /* Assume named index. */
                                                strncpy(opts->index.index, optarg, sizeof(opts->index.index));
                                                opts->index.index_len = strlen(opts->index.index);
                                                printf("Will attempt to reserve index \"%s\".\n", opts->index.index);
                                        }
                                        opts->reserve_specific_index = 1;
                                }
                                else {
                                        printf("Will reserve a random index.\n");
                                }
                                opts->reserve_index = 1;
                        }
                                break;
			case OPTION_MONITOR_CTX:
				opts->monitor_context = 1;
				opts->monitor_context_ivl = atoi(optarg);
				break;
			case OPTION_MONITOR_RCV:
				opts->monitor_receiver = 1;
				opts->monitor_receiver_ivl = atoi(optarg);
				break;
			case OPTION_MONITOR_TRANSPORT:
				if (optarg != NULL)
				{
					if (strcasecmp(optarg, "lbm") == 0)
					{
						opts->transport = lbmmon_transport_lbm_module();
					}
					else if (strcasecmp(optarg, "udp") == 0)
					{
						opts->transport = lbmmon_transport_udp_module();
					}
					else if (strcasecmp(optarg, "lbmsnmp") == 0)
					{
						opts->transport = lbmmon_transport_lbmsnmp_module();
					}
					else
					{
						++errflag;
					}
				}
				else
				{
					++errflag;
				}
				break;
			case OPTION_MONITOR_TRANSPORT_OPTS:
				if (optarg != NULL)
				{
					strncpy(opts->transport_options_string, optarg, 
							sizeof(opts->transport_options_string));
				}
				else
				{
					++errflag;
				}
				break;
			case OPTION_MONITOR_FORMAT:
				if (optarg != NULL)
				{
					if (strcasecmp(optarg, "csv") == 0)
					{
						opts->format = lbmmon_format_csv_module();
					}
					else
					{
						++errflag;
					}
				}
				else
				{
					++errflag;
				}
				break;
			case OPTION_MONITOR_FORMAT_OPTS:
				if (optarg != NULL)
				{
					strncpy(opts->format_options_string, optarg, 
							sizeof(opts->format_options_string));
				}
				else
				{
					++errflag;
				}
				break;
			case OPTION_MONITOR_APPID:
				if (optarg != NULL)
				{
					strncpy(opts->application_id_string, optarg, 
							sizeof(opts->application_id_string));
				}
				else
				{
					++errflag;
				}
				break;
			default:
				errflag++;
				break;
		}
	}
	if (errflag != 0)
	{
		fprintf(stderr, "%s\n", lbm_version());
		fprintf(stderr, Usage, argv[0]);
		exit(1);
	}
}

/*
* function to retrieve transport level loss or display transport level stats
* @num_ctx -- number of contexts
* @ctx -- contexts to retrieve transport stats for
* @print_flag -- if 1, display stats, retrieve loss otherwise
*/
lbm_ulong_t get_loss_or_print_stats(int num_ctxs, lbm_context_t * ctxs[], int print_flag){
	int ctx, nstat;
	lbm_ulong_t lost = 0;
	int have_stats, set_nstats;
 
	for (ctx = 0; ctx < num_ctxs; ctx++){
		have_stats = 0;
		while (!have_stats){
			set_nstats = nstats;
			if (lbm_context_retrieve_rcv_transport_stats(ctxs[ctx], &set_nstats, stats) == LBM_FAILURE){
				/* Double the number of stats passed to the API to be retrieved */
				/* Do so until we retrieve stats successfully or hit the max limit */
				nstats *= 2;
				if (nstats > DEFAULT_MAX_NUM_SRCS){
					fprintf(stderr, "Cannot retrieve all stats (%s).  Maximum number of sources = %d.\n",
							lbm_errmsg(), DEFAULT_MAX_NUM_SRCS);
					exit(1);
				}
				stats = (lbm_rcv_transport_stats_t *)realloc(stats,  nstats * sizeof(lbm_rcv_transport_stats_t));
				if (stats == NULL){
					fprintf(stderr, "Cannot reallocate statistics array\n");
					exit(1);
				}
			}
			else{
				have_stats = 1;
			}
		}
		/* If we get here, we have the stats */
		for (nstat = 0; nstat < set_nstats; nstat++){
			if (print_flag){
				/* Display transport level stats */
				fprintf(stdout, "stats %u/%u (ctx %u):", nstat+1, set_nstats, ctx);
				print_stats(stdout, stats[nstat]);
			}
			else{
				/* Accumlate transport level loss */
				switch (stats[nstat].type){
					case LBM_TRANSPORT_STAT_LBTRM:
						lost += stats[nstat].transport.lbtrm.lost;
						break;
					case LBM_TRANSPORT_STAT_LBTRU:
						lost += stats[nstat].transport.lbtru.lost;
						break;
				}
			}
		}

	}
	return lost;
}

/* Main for event queue processing thread(s) */
#if defined(_WIN32)
DWORD WINAPI event_queue_proccess(void *arg)
#else
void *event_queue_proccess(void *arg)
#endif
{
	struct Options *opts = &options;
	int thrdidx = *((int *)arg);
	
	if (opts->verbose)
		printf("Event Queue Thread[%d] Started\n", thrdidx);
	while (!close_recv || !end_flg_fp) {
		if (lbm_event_dispatch(evq, LBM_EVENT_QUEUE_BLOCK) == LBM_FAILURE) {
        		fprintf(stderr, "lbm_event_dispatch returned error: %s\n", lbm_errmsg());
        	        break;
		}
	}
	if (opts->verbose)
		printf("Event Queue Thread[%d] exiting\n", thrdidx);
}

/* Main application thread */
int main(int argc, char **argv)
{
	struct Options *opts = &options;
	lbm_context_t *ctxs[MAX_NUM_CTXS];
	lbm_context_attr_t * cattr;
	lbm_topic_t *topic = NULL;
	lbm_rcv_topic_attr_t *rcv_attr;
	char topicname[LBM_MSG_MAX_TOPIC_LEN];
	int i = 0;
	int ctxidx = 0;
	lbmmon_sctl_t * monctl;
	char * transport_options = NULL;
	char * format_options = NULL;
	char * application_id = NULL;
	lbm_ulong_t lost_tmp;
	lbm_rcv_t **rcvs = NULL;
	lbm_hf_rcv_t **hfrcvs = NULL;
	lbm_wildcard_rcv_t **wrcvs = NULL;
	lbm_src_t **lsrcs = NULL;
	lbm_src_topic_attr_t * stattr;
	lbm_topic_t *stopic;
	char stopicname[LBM_MSG_MAX_TOPIC_LEN];
	lbm_wildcard_rcv_attr_t * wrcv_attr;
	char response_msg[DEFAULT_RESPONSE_LEN] = {'\0'};
#if defined(_WIN32)
        HANDLE wthrdh[MAX_NUM_THREADS];
        DWORD wthrdids[MAX_NUM_THREADS];
#else
        pthread_t pthids[MAX_NUM_THREADS];
#endif
	
#if defined(_WIN32)
	{
		WSADATA wsadata;
		int status;

		/* Windows socket startup code */
		if ((status = WSAStartup(MAKEWORD(2,2),&wsadata)) != 0) {
			fprintf(stderr,"%s: WSA startup error - %d\n",argv[0],status);
			exit(1);
		}
	}
#else
	/*
	 * Ignore SIGPIPE on UNIXes which can occur when writing to a socket
	 * with only one open end point.
	 */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, INThandler);
#endif /* _WIN32 */

	/* Process command line options */
	process_cmdline(argc, argv);

	stats = (lbm_rcv_transport_stats_t *)malloc(nstats * sizeof(lbm_rcv_transport_stats_t));
	if (stats == NULL)
	{
		fprintf(stderr, "can't allocate statistics array\n");
		exit(1);
	}

	/* If sending return messages for latency, allocate buffer for message */
	if (opts->measure_latency > 1)
		rmessage = (char *) malloc(128);

	printf("Using %d context(s)\n", opts->num_ctxs);
	if (opts->monitor_context || opts->monitor_receiver)
	{
		if (strlen(opts->transport_options_string) > 0)
		{
			transport_options = opts->transport_options_string;
		}
		if (strlen(opts->format_options_string) > 0)
		{
			format_options = opts->format_options_string;
		}
		if (strlen(opts->application_id_string) > 0)
		{
			application_id = opts->application_id_string;
		}
		if (lbmmon_sctl_create(&monctl, opts->format, format_options, 
			opts->transport, transport_options) == -1)
		{
			fprintf(stderr, "lbmmon_sctl_create() failed, %s\n", lbmmon_errmsg());
			exit(1);
		}
	}

	/* Retrieve current context settings */
	if (lbm_context_attr_create(&cattr) == LBM_FAILURE) {
 		fprintf(stderr, "lbm_context_attr_create: %s\n", lbm_errmsg());
 		exit(1);
 	}

	/* Set receive socket buffers to 8MB since we expect many receivers sharing transports */
	if(opts->bufsize != 0) {
		opts->bufsize *= 1024 * 1024;
		if (lbm_context_attr_setopt(cattr, "transport_tcp_receiver_socket_buffer",
									&opts->bufsize,sizeof(opts->bufsize)) == LBM_FAILURE) {
 			fprintf(stderr, "lbm_context_attr_setopt: TCP %s\n", lbm_errmsg());
 			exit(1);
		}
		if (lbm_context_attr_setopt(cattr, "transport_lbtrm_receiver_socket_buffer",
									&opts->bufsize,sizeof(opts->bufsize)) == LBM_FAILURE) {
 			fprintf(stderr, "lbm_context_attr_setopt: LBTRM %s\n", lbm_errmsg());
 			exit(1);
		}
		if (lbm_context_attr_setopt(cattr, "transport_lbtru_receiver_socket_buffer",
									&opts->bufsize,sizeof(opts->bufsize)) == LBM_FAILURE) {
 			fprintf(stderr, "lbm_context_attr_setopt: LBTRU %s\n", lbm_errmsg());
 			exit(1);
		}
	}

	/* Create one or more LBM contexts */
	for (i = 0; i < opts->num_ctxs; i++)
	{
		if (lbm_context_create(&(ctxs[i]), cattr, NULL, NULL) == LBM_FAILURE)
		{
			fprintf(stderr, "lbm_context_create: %s\n", lbm_errmsg());
			exit(1);
		}
		if (opts->monitor_context)
		{
			char appid[1024];
			char * appid_ptr = NULL;
			if (application_id != NULL)
			{
				snprintf(appid, sizeof(appid), "%s(%d)", application_id, i);
				appid_ptr = appid;
			}
			if (lbmmon_context_monitor(monctl, ctxs[i], appid_ptr, 
									   opts->monitor_context_ivl) == -1)
			{
				fprintf(stderr, "lbmmon_context_monitor() failed, %s\n", lbmmon_errmsg());
				exit(1);
			}
		}
	}
	
	/* After a context gets created, the attributes can be discarded */
	lbm_context_attr_delete(cattr);;

	/* Allocate receiver array */
	if ((rcvs = malloc(sizeof(lbm_rcv_t *) * MAX_NUM_RCVS)) == NULL) {
		fprintf(stderr, "could not allocate receivers array\n");
		exit(1);
	}
	/* Allocate HF rcv array if using HF option */
	if (opts->use_hf) {
		if ((hfrcvs = malloc(sizeof(lbm_hf_rcv_t *) * MAX_NUM_RCVS)) == NULL) {
                        fprintf(stderr, "could not allocate HF receivers array\n");
                        exit(1);
                }
	}
	/* Allocate wildcard receiver array if using wilcard option */
	if (opts->wildcard) {
		if ((wrcvs = malloc(sizeof(lbm_wildcard_rcv_t *) * MAX_NUM_RCVS)) == NULL) {
                        fprintf(stderr, "could not allocate wildcard receivers array\n");
                        exit(1);
                }
		/* Retrieve the current wildcard receiver attributes */
      	        if (lbm_wildcard_rcv_attr_create(&wrcv_attr) == LBM_FAILURE) {
               	        fprintf(stderr, "lbm_wildcard_rcv_attr_create: %s\n", lbm_errmsg());
               		exit(1);
        	}
        }

	/* Allocate return sources for round trip latency measurements */
	if (opts->measure_latency > 1)
	{
		if ((lsrcs = malloc(sizeof(lbm_src_t *) * MAX_NUM_RCVS)) == NULL)
		{
			fprintf(stderr, "could not allocate latency sources\n");
			exit(1);
		}
	}

#if !defined(_WIN32)
	signal(SIGHUP, SigHupHandler);
	signal(SIGUSR1, SigUsr1Handler);
	signal(SIGUSR2, SigUsr2Handler);
#endif

	/* init rcv topic attributes */
	if (lbm_rcv_topic_attr_create(&rcv_attr) == LBM_FAILURE) {
		fprintf(stderr, "lbm_rcv_topic_attr_create: %s\n", lbm_errmsg());
		exit(1);
	}

	if (opts->eventq)
	{
		/* Allocate event queue with associated monitor callback */
		if (lbm_event_queue_create(&evq, evq_monitor, NULL, NULL) == LBM_FAILURE) {
			fprintf(stderr, "lbm_event_queue_create: %s\n", lbm_errmsg());
			exit(1);
		}
	}
	
	/* Wildcard receivers and HF not compatible */
	if (opts->use_hf && opts->wildcard)
	{
		fprintf(stderr, "Hot-Failover and Wildcard receivers not compatible, exiting\n");
		exit(1);
	}

	/* Create all the receivers */
	if (!opts->use_hf && !opts->wildcard)
		printf("Creating %d receivers\n", opts->num_rcvs);
	else if (opts->use_hf)
		printf("Creating %d HF receivers\n", opts->num_rcvs);
	else
		printf("Creating %d wildcard receivers\n", opts->num_rcvs);

	if (opts->measure_latency > 1)
        	printf("Creating %d sources for round trip latency measurements\n", opts->num_rcvs);

	ctxidx = 0;
	for (i = 0; i < opts->num_rcvs; i++) {
		/* Allocate topicname */
		if (!opts->wildcard)
                        sprintf(topicname, "%s.%d", opts->topicroot, (i + opts->initial_topic_number));
		else
			sprintf(topicname, "%s.%d.*", opts->topicroot, (i + opts->initial_topic_number));

		/*
		 * Create return sources if measuring round trip latency 
		 * We do this first so we can assign the src to the clientd of each receiver
		 */
                if (opts->measure_latency > 1)
                {
                        if (lbm_src_topic_attr_create(&stattr) != 0)
                        {
                                fprintf(stderr, "lbm_src_topic_attr_create: %s\n", lbm_errmsg());
                                exit(1);
                        }

                        sprintf(stopicname, "%s.rtt", topicname);
                        if (lbm_src_topic_alloc(&stopic, ctxs[ctxidx], stopicname, stattr))
                        {
                                printf("lbm_src_topic_alloc: %s\n", lbm_errmsg());
                                exit(1);
                        }
		
      			if (lbm_src_create(&(lsrcs[i]), ctxs[ctxidx], stopic, NULL, NULL, NULL))
      			{
          			printf("lbm_src_create: %s\n", lbm_errmsg());
          			exit(1);
      			}
                }
	
		if (!opts->wildcard)
		{
			topic = NULL;
			/* First lookup the desired topic */
			if (lbm_rcv_topic_lookup(&topic, ctxs[ctxidx], topicname, rcv_attr) == LBM_FAILURE) {
				fprintf(stderr, "lbm_rcv_topic_alloc: %s\n", lbm_errmsg());
				exit(1);
			}
		}
		else {
			/* Create the wildcard receiver using the default (or configed) pattern type */
        		if (lbm_wildcard_rcv_create(&(wrcvs[i]), ctxs[ctxidx], topicname, NULL, wrcv_attr, rcv_handle_msg, 
			    opts->measure_latency > 1 ? lsrcs[i] : NULL, opts->eventq ? evq : NULL) == LBM_FAILURE) {
                		fprintf(stderr, "lbm_wildcard_rcv_create: %s\n", lbm_errmsg());
                		exit(1);
        		}
		}

		/*
		 * Create receiver passing in the looked up topic info.
		 * We use the same callback function for data received.
		 */
		if (!opts->use_hf && !opts->wildcard) {
			if (lbm_rcv_create(&(rcvs[i]), ctxs[ctxidx], topic, rcv_handle_msg, 
			    opts->measure_latency > 1 ? lsrcs[i] : NULL, opts->eventq ? evq : NULL) == LBM_FAILURE) {
				fprintf(stderr, "lbm_rcv_create: %s\n", lbm_errmsg());
				exit(1);
			}
		}
		else if (opts->use_hf) {
			if (lbm_hf_rcv_create(&(hfrcvs[i]), ctxs[ctxidx], topic, rcv_handle_msg, 
			    opts->measure_latency > 1 ? lsrcs[i] : NULL, opts->eventq ? evq : NULL) == LBM_FAILURE) {
                                fprintf(stderr, "lbm_hf_rcv_create: %s\n", lbm_errmsg());
                                exit(1);
                        }
			/* An HF receiver is associated with an automatically created LBM receiver */
			rcvs[i] = lbm_rcv_from_hf_rcv(hfrcvs[i]);
		}

		/* Assign Spectrum channel */
		if (opts->channel)
		{
			if (!opts->wildcard)
				lbm_rcv_subscribe_channel(rcvs[i], opts->channel, NULL, NULL);
			else
				lbm_wildcard_rcv_subscribe_channel(wrcvs[i], opts->channel, NULL, NULL);
		}

		if (opts->monitor_receiver && !opts->wildcard)
		{
			char appid[1024];
			char * appid_ptr = NULL;
			if (application_id != NULL)
			{
				snprintf(appid, sizeof(appid), "%s(%d)", application_id, i);
				appid_ptr = appid;
			}
			if (lbmmon_rcv_monitor(monctl, rcvs[i], appid_ptr, opts->monitor_receiver_ivl) == -1)
			{
				fprintf(stderr, "lbmmon_receiver_monitor() failed, %s\n", lbmmon_errmsg());
				exit(1);
			}
		}
		if (opts->verbose)
			printf("Created receiver %d - '%s'\n",i,topicname);
		if (i > 1 && (i % 1000) == 0)
			printf("Created %d receivers\n", i);
		ctxidx++;
		if (ctxidx >= opts->num_ctxs)
			ctxidx = 0;
	}
	printf("Created %d receivers. Will start calculating aggregate throughput.\n", opts->num_rcvs);

	/* Need to enable event queue for multiple event queue threads */
	if (opts->threads && !opts->eventq)
	{
		fprintf(stderr, "Event queue threads specified, but not the event queue, invalid options\n");
		exit(1);
	}

	/* Create event queue threads */
	if (opts->eventq)
	{
		int num_thrds = opts->threads;
		if (num_thrds == 0) 
			num_thrds = 1;
		for (i = 0; i < num_thrds; i++)
		{
			thrdidxs[i] = i;
#if defined(_WIN32)
                	if ((wthrdh[i] = CreateThread(NULL, 0, event_queue_proccess, &(thrdidxs[i]), 0, &(wthrdids[i]))) == NULL) {
                	        fprintf(stderr, "could not create thread\n");
                	        exit(1);
                	}
#else
                	if (pthread_create(&(pthids[i]), NULL, event_queue_proccess, &(thrdidxs[i])) != 0) {
                	        fprintf(stderr, "could not spawn thread\n");
                	        exit(1);
                	}
#endif
		}
	}
	
	/* Delete rcv topic attributes */
	lbm_rcv_topic_attr_delete(rcv_attr);

	/* Initialize end flag file */
	if (opts->end_flg_file)
		unlink(opts->end_flg_file);

	/* Set metrics printing callback */
	if (opts->print_metrics > 0)
	{
		if (lbm_schedule_timer_recurring(ctxs[0], (lbm_timer_cb_proc)print_metrics, NULL, NULL, opts->print_metrics) == -1)
		{
			fprintf(stderr, "Error scheduling timer callback\n");
       	        	exit(1);
		}
	}

	/* Sleep/Wakeup every second and print out bandwidth stats. */
	end_flg_fp = NULL;
	while (end_flg_fp == NULL) {
		struct timeval starttv, endtv;

		current_tv(&starttv);
		
		SLEEP_SEC(1);

		/* Calculate aggregate transport level loss */
		/* Pass 0 for the print flag indicating interested in retrieving loss stats */
		lost = get_loss_or_print_stats(opts->num_ctxs, ctxs, 0);

		lost_tmp = lost;
		if (last_lost <= lost){
			lost -= last_lost;
		}
		else{
			lost = 0;
		}
		last_lost = lost_tmp;

		current_tv(&endtv);
		endtv.tv_sec -= starttv.tv_sec;
		endtv.tv_usec -= starttv.tv_usec;
		normalize_tv(&endtv);

		if (!opts->ascii)
			print_bw(stdout, &endtv, msg_count, byte_count, unrec_count, lost, rxs, otrs);

		msg_count = 0;
		byte_count = 0;
		unrec_count = 0;
		rxs = 0;
		otrs = 0;
        	lstream = 0;

		if (opts->pstats){
			/* Display transport level statistics */
			/* Pass opts->pstats for the print flag indicating interested in displaying stats */
            		//print_perf_stats();
			get_loss_or_print_stats(opts->num_ctxs, ctxs, opts->pstats);
		}

		if (opts->end_flg_file)
			end_flg_fp = fopen(opts->end_flg_file, "r");
		if (close_recv)
			break;
	}
	if (end_flg_fp != NULL) {  /* in case break loop for other reason */
		fclose(end_flg_fp);
		printf("%s detected, cleaning up....\n", opts->end_flg_file);
	}

	printf("Lingering for %d seconds...\n", opts->linger);
	SLEEP_SEC(opts->linger);

	if (opts->monitor_context || opts->monitor_receiver)
	{
		if (opts->monitor_context)
		{
			for (i = 0; i < opts->num_ctxs; ++i)
			{
				if (lbmmon_context_unmonitor(monctl, ctxs[i]) == -1)
				{
					fprintf(stderr, "lbmmon_context_unmonitor() failed, %s\n", lbmmon_errmsg());
					exit(1);
				}
			}
		}
		if (opts->monitor_receiver)
		{
			for (i = 0; i < opts->num_rcvs; ++i)
			{
				if (lbmmon_rcv_unmonitor(monctl, rcvs[i]) == -1)
				{
					fprintf(stderr, "lbmmon_rcv_unmonitor() failed, %s\n", lbmmon_errmsg());
					exit(1);
				}
			}
		}
		if (lbmmon_sctl_destroy(monctl) == -1)
		{
			fprintf(stderr, "lbmmon_sctl_destoy() failed(), %s\n", lbmmon_errmsg());
			exit(1);
		}
	}

	/*
	 * Not strictly necessary (nor reached) in this example, but this is
	 * how to delete receivers and tear down a context.
	 */
	printf("Deleting receivers....\n");
	for (i = 0; i < opts->num_rcvs;) {
		if (!opts->use_hf) {
			lbm_rcv_delete(rcvs[i]);
			rcvs[i] = NULL;
		}
		else {
			lbm_hf_rcv_delete(hfrcvs[i]);
			hfrcvs[i] = NULL;
		}
		if (i > 1 && (i % 1000) == 0)
			printf("Deleted %d receivers\n",i);
		i++;
	}
	for (i = 0; i < opts->num_ctxs; i++) {
		lbm_context_delete(ctxs[i]);
		ctxs[i] = NULL;
	}
	free(rcvs);
	printf("Quitting.... received %u messages", total_msg_count);
	if (total_unrec_count > 0 || total_burst_loss > 0) {
		printf(", %u msgs unrecovered, %u loss bursts", total_unrec_count, total_burst_loss);
	}
	if (opts->eventq) {
		int num_thrds = opts->threads;
		if (num_thrds == 0)
			num_thrds = 1;
		/* Be sure the loops end */
		close_recv = 1;
		/* Unblock event queues and return threads */
		for (i = 0; i < num_thrds; i++) 
			lbm_event_dispatch_unblock(evq);
		
		for (i = 0; i < num_thrds; i++)
                {
#if defined(_WIN32)
     	  		WaitForSingleObject(wthrdh[i], INFINITE);
#else
                        pthread_join(pthids[i], NULL);
#endif
		}

		lbm_event_queue_delete(evq);
	}
	
	free(rmessage);

	printf("\n");
	return 0;
}

