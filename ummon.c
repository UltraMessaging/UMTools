/*
  lbmmoncsv.c: example LBM monitoring application, outputting stats to csv files.

  Copyright (c) 2005-2014 Informatica Corporation  Permission is granted to licensees to use
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
#if defined(__TANDEM) && defined(HAVE_TANDEM_SPT)
	#include <ktdmtyp.h>
	#include <spthread.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <sys/timeb.h>
	#define strcasecmp stricmp
	#define snprintf _snprintf
	typedef int ssize_t;
#else
	#include <unistd.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <signal.h>
	#include <sys/utsname.h>
	#if defined(__TANDEM)
		#include <strings.h>
	#endif
#endif
#if defined(__VMS)
	typedef int socklen_t;
#endif
#include "replgetopt.h"
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include "monmodopts.h"

#ifdef _WIN32
	#define LBMMONUDP_INVALID_HANDLE INVALID_SOCKET
	#define LBMMONUDP_SOCKET_ERROR SOCKET_ERROR
#else
	#define LBMMONUDP_INVALID_HANDLE -1
	#define LBMMONUDP_SOCKET_ERROR -1
#endif
#ifndef INADDR_NONE
	#define INADDR_NONE ((in_addr_t) 0xffffffff)
#endif

static const char *rcsid_example_lbmmoncsv = "$Id: //UMprod/REL_6_7_1/29West/lbm/src/example/lbmmoncsv.c#1 $";

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

const char Purpose[] = "Purpose: Receive LBM statistics and write to csv files.";
const char Usage[] =
"Usage: %s [options] file-root\n"
"Available options:\n"
"  -f, --format=FMT           use monitor format module FMT\n"
"                             FMT may be `csv'\n"
"      --format-opts=OPTS     use OPTS as format module options\n"
"  -h, --help                 display this help and exit\n"
"  -t, --transport=TRANS      use monitor transport module TRANS\n"
"                             TRANS may be `lbm' or `udp', default is `lbm'\n"
"      --transport-opts=OPTS  use OPTS as transport module options\n"
"  -u, --use-header           use transport header in csv file\n"
"                             default is tcp\n"
"                             0 = TCP\n"
"                             1 = LBTRM\n"
"                             2 = LBTRU\n"
"                             3 = LBTIPC\n"
"                             4 = LBTSMX\n"
"                             5 = LBTRDMA\n"

MONMODULEOPTS_RECEIVER;

const char OptionString[] = "f:hit:u:";
#define OPTION_MONITOR_TRANSPORT_OPTS 0
#define OPTION_MONITOR_FORMAT_OPTS 1
const struct option OptionTable[] =
{
	{ "format", required_argument, NULL, 'f' },
	{ "format-opts", required_argument, NULL, OPTION_MONITOR_FORMAT_OPTS },
	{ "help", no_argument, NULL, 'h' },
	{ "transport", required_argument, NULL, 't' },
	{ "transport-opts", required_argument, NULL, OPTION_MONITOR_TRANSPORT_OPTS },
	{ "use-header", required_argument, NULL, 'u' },
	{ NULL, 0, NULL, 0 }
};

/* Data version constants */
#define LBMMONUDP_TCP_SRC_V1 1
#define LBMMONUDP_TCP_RCV_V1 2
#define LBMMONUDP_LBTRM_SRC_V1 3
#define LBMMONUDP_LBTRM_RCV_V1 4
#define LBMMONUDP_LBTRU_SRC_V1 5
#define LBMMONUDP_LBTRU_RCV_V1 6
#define LBMMONUDP_EVQ_V1 7
#define LBMMONUDP_CTX_V1 8
#define LBMMONUDP_CTX_IM_LBTRM_SRC_V1 9
#define LBMMONUDP_CTX_IM_LBTRM_RCV_V1 10
#define LBMMONUDP_LBTIPC_SRC_V1 11
#define LBMMONUDP_LBTIPC_RCV_V1 12
#define LBMMONUDP_LBTRDMA_SRC_V1 13
#define LBMMONUDP_LBTRDMA_RCV_V1 14
#define LBMMONUDP_LBTSMX_SRC_V1 15
#define LBMMONUDP_LBTSMX_RCV_V1 16

/* Current data versions */
#define LBMMONUDP_TCP_SRC_VER LBMMONUDP_TCP_SRC_V1
#define LBMMONUDP_TCP_RCV_VER LBMMONUDP_TCP_RCV_V1
#define LBMMONUDP_LBTRM_SRC_VER LBMMONUDP_LBTRM_SRC_V1
#define LBMMONUDP_LBTRM_RCV_VER LBMMONUDP_LBTRM_RCV_V1
#define LBMMONUDP_LBTRU_SRC_VER LBMMONUDP_LBTRU_SRC_V1
#define LBMMONUDP_LBTRU_RCV_VER LBMMONUDP_LBTRU_RCV_V1
#define LBMMONUDP_EVQ_VER LBMMONUDP_EVQ_V1
#define LBMMONUDP_CTX_VER LBMMONUDP_CTX_V1
#define LBMMONUDP_CTX_IM_LBTRM_SRC_VER LBMMONUDP_CTX_IM_LBTRM_SRC_V1
#define LBMMONUDP_CTX_IM_LBTRM_RCV_VER LBMMONUDP_CTX_IM_LBTRM_RCV_V1
#define LBMMONUDP_LBTIPC_SRC_VER LBMMONUDP_LBTIPC_SRC_V1
#define LBMMONUDP_LBTIPC_RCV_VER LBMMONUDP_LBTIPC_RCV_V1
#define LBMMONUDP_LBTSMX_SRC_VER LBMMONUDP_LBTSMX_SRC_V1
#define LBMMONUDP_LBTSMX_RCV_VER LBMMONUDP_LBTSMX_RCV_V1
#define LBMMONUDP_LBTRDMA_SRC_VER LBMMONUDP_LBTRDMA_SRC_V1
#define LBMMONUDP_LBTRDMA_RCV_VER LBMMONUDP_LBTRDMA_RCV_V1

/* Default values */
#define LBM_CTX_WRITE 0
#define LBM_RCV_WRITE 1
#define LBM_SRC_WRITE 2
#define LBM_EVQ_WRITE 3

#define LBM_CTX_HEADER "format, time, ip, app, source, tr_dgrams_sent, tr_bytes_sent, tr_dgrams_rcved, tr_bytes_rcved, tr_dgrams_dropped_ver, tr_dgrams_dropped_type, tr_dgrams_dropped_malformed, tr_dgrams_send_failed, tr_src_topics, tr_rcv_topics, tr_rcv_unresolved_topics, lbtrm_unknown_msgs_rcved, lbtru_unknown_msgs_rcved, send_blocked, send_would_block, resp_blocked, resp_would_block, uim_dup_msgs_rcved, uim_msgs_no_stream_rcved\n"
#define LBM_RCV_LBTRM_HEADER "format, time, ip, app, source, msgs_rcved, bytes_rcved, nak_pckts_sent, naks_sent, lost, ncfs_ignored, ncfs_shed, ncfs_rx_delay, ncfs_unknown, nak_stm_min, nak_stm_mean, nak_stm_max, nak_tx_min, nak_tx_mean, nak_tx_max, duplicate_data, unrecoverable_txw, unrecoverable_tmo, lbm_msgs_rcved, lbm_msgs_no_topic_rcved, lbm_reqs_rcved, dgrams_dropped_size, dgrams_dropped_type, dgrams_dropped_version, dgram_dropped_hdr, dgrams_dropped_other, out_of_order\n"
#define LBM_RCV_LBTRU_HEADER "format, time, ip, app, source, msgs_rcved, bytes_rcved, nak_pckts_sent, naks_sent, lost, ncfs_ignored, ncfs_shed, ncfs_rx_delay, ncfs_unknown, nak_stm_min, nak_stm_mean, nak_stm_max, nak_tx_min, nak_tx_mean, nak_tx_max, duplicate_data, unrecoverable_txw, unrecoverable_tmo, lbm_msgs_rcved, lbm_msgs_no_topic_rcved, lbm_reqs_rcved, dgrams_dropped_size, dgrams_dropped_type, dgrams_dropped_version, dgram_dropped_hdr, dgrams_dropped_other\n"
#define LBM_RCV_TCP_HEADER "format, time, ip, app, source, bytes_rcved, lbm_msgs_rcved, lbm_msgs_no_topic_rcved, lbm_reqs_rcved\n"
#define LBM_RCV_IPC_HEADER "format, time, ip, app, source, msgs_rcved, bytes_rcved, lbm_msgs_rcved, lbm_msgs_no_topic_rcved, lbm_reqs_rcved\n"
#define LBM_RCV_SMX_HEADER "format, time, ip, app, source, msgs_rcved, bytes_rcved, lbm_msgs_rcved, lbm_msgs_no_topic_rcved, reserved\n"
#define LBM_RCV_RDMA_HEADER "format, time, ip, app, source, msgs_rcved, bytes_rcved, lbm_msgs_rcved, lbm_msgs_no_topic_rcved, lbm_reqs_rcved\n"
#define LBM_SRC_LBTRM_HEADER "format, time, ip, app, source, msgs_sent, bytes_sent, txw_msgs, txw_bytes, nak_pckts_rcved, naks_rcved, naks_ignored, naks_shed, naks_rx_delay_ignored, rxs_sent, rctlr_data_msgs, rctlr_rx_msgs, rx_bytes_sent\n"
#define LBM_SRC_LBTRU_HEADER "format, time, ip, app, source, msgs_sent, bytes_sent, nak_pckts_rcved, naks_rcved, naks_ignored, naks_shed, naks_rx_delay_ignored, rxs_sent, num_clients, rx_bytes_sent\n"
#define LBM_SRC_TCP_HEADER "format, time, ip, app, source, num_clients, bytes_buffered\n"
#define LBM_SRC_IPC_HEADER "format, time, ip, app, source, num_clients, msgs_sent, bytes_sent\n"
#define LBM_SRC_SMX_HEADER "format, time, ip, app, source, num_clients, msgs_sent, bytes_sent\n"
#define LBM_SRC_RDMA_HEADER "format, time, ip, app, source, num_clients, msgs_sent, bytes_sent\n"
#define LBM_EVQ_HEADER "format, time, ip, app, source, data_msgs, data_msgs_tot, data_msgs_svc_min, data_msgs_svc_mean, data_msgs_svc_max, resp_msgs, resp_msgs_tot, resp_msgs_svs_min, resp_msgs_svc_mean, resp_msgs_svc_max, topicless_im_msgs, topicless_im_msgs_tot, topicless_im_msgs_min, topicless_im_msgs_mean, topicless_im_msgs_max, wrcv_msgs, wrcv_msgs_tot, wrcv_msgs_svc_min, wrcv_msgs_svc_mean, wrcv_msgs_svc_max, io_events, io_events_tot, io_events_svc_min, io_events_svc_mean, io_events_svc_max, timer_events, timer_events_tot, timer_events_svc_min, timer_events_svc_mean, timer_events_svc_max, source_events, source_events_tot, source_events_svc_min, source_events_svc_mean, source_events_svc_max, unblock_events, unblock_events_tot, cancel_events, cancel_events_tot, cancel_events_svc_min, cancel_events_svc_mean, cancel_events_svc_max, context_source_events, context_source_events_tot, context_source_events_svc_min, context_source_events_svc_mean, context_source_events_svc_max, events, events_tot, age_min, age_mean, age_max\n"

#define UM_MON_VERSION "0.1"

/* Global variables */
unsigned int Terminate = 0;
unsigned int Force32Bit = 0;
struct sockaddr_in Peer;
unsigned long int ValueMask = ULONG_MAX;
FILE *fp_ctx, *fp_src, *fp_rcv, *fp_evq;
unsigned long int MessagesReceived = 0;

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

void SignalHandler(int signo)
{
	Terminate = 1;
}

unsigned long int preprocess_value(unsigned long int Value)
{
	return (Value & ValueMask);
}

void write_stats(const char * Buffer, int type)
{
	switch (type) {
		case LBM_CTX_WRITE:
			fprintf(fp_ctx, Buffer);
			break;
		case LBM_RCV_WRITE:
			fprintf(fp_rcv, Buffer);
			break;
		case LBM_SRC_WRITE:
			fprintf(fp_src, Buffer);
			break;
		case LBM_EVQ_WRITE:
			fprintf(fp_evq, Buffer);
			break;
	}
}

void write_rcv_header(int header)
{
	switch (header) {
		case 0:
			fprintf(fp_rcv, LBM_RCV_TCP_HEADER);
			break;
		case 1:
			fprintf(fp_rcv, LBM_RCV_LBTRM_HEADER);
			break;
		case 2:
			fprintf(fp_rcv, LBM_RCV_LBTRU_HEADER);
			break;
		case 3:
			fprintf(fp_rcv, LBM_RCV_IPC_HEADER);
			break;
		case 4:
			fprintf(fp_rcv, LBM_RCV_SMX_HEADER);
			break;
		case 5: 
			fprintf(fp_rcv, LBM_RCV_RDMA_HEADER);
			break;
		default:
			fprintf(stderr, "Invalid use-header option\n");
			exit(1);
			break;
	}
}

void write_src_header(int header)
{
        switch (header) {
                case 0:
                        fprintf(fp_src, LBM_SRC_TCP_HEADER);
                        break;
                case 1:
                        fprintf(fp_src, LBM_SRC_LBTRM_HEADER);
                        break;
                case 2:
                        fprintf(fp_src, LBM_SRC_LBTRU_HEADER);
                        break;
                case 3:
                        fprintf(fp_src, LBM_SRC_IPC_HEADER);
                        break;
                case 4:
                        fprintf(fp_src, LBM_SRC_SMX_HEADER);
                        break;
                case 5: 
			fprintf(fp_src, LBM_SRC_RDMA_HEADER);
                        break;
                default:
                        fprintf(stderr, "Invalid use-header option\n");
                        exit(1);
                        break;
        }
}


void format_common_data(char * Buffer, size_t Size, unsigned int Format, const void * AttributeBlock, const char * Source)
{
	time_t timestamp = 0;
	struct in_addr addr;
	char appid[256];

	lbmmon_attr_get_timestamp(AttributeBlock, &timestamp);
	memset(appid, 0, sizeof(appid));
	lbmmon_attr_get_appsourceid(AttributeBlock, appid, sizeof(appid));
	lbmmon_attr_get_ipv4sender(AttributeBlock, (lbm_uint_t *) &(addr.s_addr));
	snprintf(Buffer,
			Size,
			"%u,%d,\"%s\",\"%s\",\"%s\",",
			Format,
			(int) timestamp,
			inet_ntoa(addr),
			appid,
			(Source == NULL) ? " " : Source);
}

void rcv_statistics_cb(const void * AttributeBlock, const lbm_rcv_transport_stats_t * Statistics, void * ClientData)
{
	unsigned int format;
	char buffer[4096];
	char * ptr;
	size_t remaining_len;
	lbm_ulong_t source;

	if (lbmmon_attr_get_source(AttributeBlock, &source) != 0)
	{
		source = LBMMON_ATTR_SOURCE_NORMAL;
	}

	memset(buffer, 0, sizeof(buffer));
	switch (Statistics->type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			format = LBMMONUDP_TCP_RCV_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTRM:
			switch (source)
			{
				case LBMMON_ATTR_SOURCE_IM:
					format = LBMMONUDP_CTX_IM_LBTRM_RCV_VER;
					break;
				default:
					format = LBMMONUDP_LBTRM_RCV_VER;
					break;
			}
			break;

		case LBM_TRANSPORT_STAT_LBTRU:
			format = LBMMONUDP_LBTRU_RCV_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTIPC:
			format = LBMMONUDP_LBTIPC_RCV_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTSMX:
			format = LBMMONUDP_LBTSMX_RCV_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTRDMA:
			format = LBMMONUDP_LBTRDMA_RCV_VER;
			break;

		default:
			return;
	}
	++MessagesReceived;
	format_common_data(buffer, sizeof(buffer), format, AttributeBlock, Statistics->source);
	ptr = &(buffer[strlen(buffer)]);
	remaining_len = sizeof(buffer) - strlen(buffer);
	switch (Statistics->type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.tcp.bytes_rcved),
					 preprocess_value(Statistics->transport.tcp.lbm_msgs_rcved),
					 preprocess_value(Statistics->transport.tcp.lbm_msgs_no_topic_rcved),
					 preprocess_value(Statistics->transport.tcp.lbm_reqs_rcved));
			break;

		case LBM_TRANSPORT_STAT_LBTRM:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtrm.msgs_rcved),
					 preprocess_value(Statistics->transport.lbtrm.bytes_rcved),
					 preprocess_value(Statistics->transport.lbtrm.nak_pckts_sent),
					 preprocess_value(Statistics->transport.lbtrm.naks_sent),
					 preprocess_value(Statistics->transport.lbtrm.lost),
					 preprocess_value(Statistics->transport.lbtrm.ncfs_ignored),
					 preprocess_value(Statistics->transport.lbtrm.ncfs_shed),
					 preprocess_value(Statistics->transport.lbtrm.ncfs_rx_delay),
					 preprocess_value(Statistics->transport.lbtrm.ncfs_unknown),
					 preprocess_value(Statistics->transport.lbtrm.nak_stm_min),
					 preprocess_value(Statistics->transport.lbtrm.nak_stm_mean),
					 preprocess_value(Statistics->transport.lbtrm.nak_stm_max),
					 preprocess_value(Statistics->transport.lbtrm.nak_tx_min),
					 preprocess_value(Statistics->transport.lbtrm.nak_tx_mean),
					 preprocess_value(Statistics->transport.lbtrm.nak_tx_max),
					 preprocess_value(Statistics->transport.lbtrm.duplicate_data),
					 preprocess_value(Statistics->transport.lbtrm.unrecovered_txw),
					 preprocess_value(Statistics->transport.lbtrm.unrecovered_tmo),
					 preprocess_value(Statistics->transport.lbtrm.lbm_msgs_rcved),
					 preprocess_value(Statistics->transport.lbtrm.lbm_msgs_no_topic_rcved),
					 preprocess_value(Statistics->transport.lbtrm.lbm_reqs_rcved),
					 preprocess_value(Statistics->transport.lbtrm.dgrams_dropped_size),
					 preprocess_value(Statistics->transport.lbtrm.dgrams_dropped_type),
					 preprocess_value(Statistics->transport.lbtrm.dgrams_dropped_version),
					 preprocess_value(Statistics->transport.lbtrm.dgrams_dropped_hdr),
					 preprocess_value(Statistics->transport.lbtrm.dgrams_dropped_other),
					 preprocess_value(Statistics->transport.lbtrm.out_of_order));
			break;

		case LBM_TRANSPORT_STAT_LBTRU:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtru.msgs_rcved),
					 preprocess_value(Statistics->transport.lbtru.bytes_rcved),
					 preprocess_value(Statistics->transport.lbtru.nak_pckts_sent),
					 preprocess_value(Statistics->transport.lbtru.naks_sent),
					 preprocess_value(Statistics->transport.lbtru.lost),
					 preprocess_value(Statistics->transport.lbtru.ncfs_ignored),
					 preprocess_value(Statistics->transport.lbtru.ncfs_shed),
					 preprocess_value(Statistics->transport.lbtru.ncfs_rx_delay),
					 preprocess_value(Statistics->transport.lbtru.ncfs_unknown),
					 preprocess_value(Statistics->transport.lbtru.nak_stm_min),
					 preprocess_value(Statistics->transport.lbtru.nak_stm_mean),
					 preprocess_value(Statistics->transport.lbtru.nak_stm_max),
					 preprocess_value(Statistics->transport.lbtru.nak_tx_min),
					 preprocess_value(Statistics->transport.lbtru.nak_tx_mean),
					 preprocess_value(Statistics->transport.lbtru.nak_tx_max),
					 preprocess_value(Statistics->transport.lbtru.duplicate_data),
					 preprocess_value(Statistics->transport.lbtru.unrecovered_txw),
					 preprocess_value(Statistics->transport.lbtru.unrecovered_tmo),
					 preprocess_value(Statistics->transport.lbtru.lbm_msgs_rcved),
					 preprocess_value(Statistics->transport.lbtru.lbm_msgs_no_topic_rcved),
					 preprocess_value(Statistics->transport.lbtru.lbm_reqs_rcved),
					 preprocess_value(Statistics->transport.lbtru.dgrams_dropped_size),
					 preprocess_value(Statistics->transport.lbtru.dgrams_dropped_type),
					 preprocess_value(Statistics->transport.lbtru.dgrams_dropped_version),
					 preprocess_value(Statistics->transport.lbtru.dgrams_dropped_hdr),
					 preprocess_value(Statistics->transport.lbtru.dgrams_dropped_sid),
					 preprocess_value(Statistics->transport.lbtru.dgrams_dropped_hdr));
			break;

		case LBM_TRANSPORT_STAT_LBTIPC:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtipc.msgs_rcved),
					 preprocess_value(Statistics->transport.lbtipc.bytes_rcved),
					 preprocess_value(Statistics->transport.lbtipc.lbm_msgs_rcved),
					 preprocess_value(Statistics->transport.lbtipc.lbm_msgs_no_topic_rcved),
					 preprocess_value(Statistics->transport.lbtipc.lbm_reqs_rcved));
			break;

		case LBM_TRANSPORT_STAT_LBTSMX:
			snprintf(ptr,
					remaining_len,
					"%lu,%lu,%lu,%lu,%lu\n",
					preprocess_value(Statistics->transport.lbtsmx.msgs_rcved),
					preprocess_value(Statistics->transport.lbtsmx.bytes_rcved),
					preprocess_value(Statistics->transport.lbtsmx.lbm_msgs_rcved),
					preprocess_value(Statistics->transport.lbtsmx.lbm_msgs_no_topic_rcved),
					preprocess_value(Statistics->transport.lbtsmx.reserved1));
			break;

		case LBM_TRANSPORT_STAT_LBTRDMA:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtrdma.msgs_rcved),
					 preprocess_value(Statistics->transport.lbtrdma.bytes_rcved),
					 preprocess_value(Statistics->transport.lbtrdma.lbm_msgs_rcved),
					 preprocess_value(Statistics->transport.lbtrdma.lbm_msgs_no_topic_rcved),
					 preprocess_value(Statistics->transport.lbtrdma.lbm_reqs_rcved));
			break;
	}
	write_stats(buffer, LBM_RCV_WRITE);
}

void src_statistics_cb(const void * AttributeBlock, const lbm_src_transport_stats_t * Statistics, void * ClientData)
{
	unsigned int format;
	char buffer[4096];
	char * ptr;
	size_t remaining_len;
	lbm_ulong_t source;

	if (lbmmon_attr_get_source(AttributeBlock, &source) != 0)
	{
		source = LBMMON_ATTR_SOURCE_NORMAL;
	}

	memset(buffer, 0, sizeof(buffer));
	switch (Statistics->type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			format = LBMMONUDP_TCP_SRC_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTRM:
			switch (source)
			{
				case LBMMON_ATTR_SOURCE_IM:
					format = LBMMONUDP_CTX_IM_LBTRM_SRC_VER;
					break;
	
				default:
					format = LBMMONUDP_LBTRM_SRC_VER;
					break;
			}
			break;

		case LBM_TRANSPORT_STAT_LBTRU:
			format = LBMMONUDP_LBTRU_SRC_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTIPC:
			format = LBMMONUDP_LBTIPC_SRC_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTSMX:
			format = LBMMONUDP_LBTSMX_SRC_VER;
			break;

		case LBM_TRANSPORT_STAT_LBTRDMA:
			format = LBMMONUDP_LBTRDMA_SRC_VER;
			break;

		default:
			return;
	}
	++MessagesReceived;
	format_common_data(buffer, sizeof(buffer), format, AttributeBlock, Statistics->source);
	ptr = &(buffer[strlen(buffer)]);
	remaining_len = sizeof(buffer) - strlen(buffer);
	switch (Statistics->type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu\n",
					 preprocess_value(Statistics->transport.tcp.num_clients),
					 preprocess_value(Statistics->transport.tcp.bytes_buffered));
			break;

		case LBM_TRANSPORT_STAT_LBTRM:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtrm.msgs_sent),
					 preprocess_value(Statistics->transport.lbtrm.bytes_sent),
					 preprocess_value(Statistics->transport.lbtrm.txw_msgs),
					 preprocess_value(Statistics->transport.lbtrm.txw_bytes),
					 preprocess_value(Statistics->transport.lbtrm.nak_pckts_rcved),
					 preprocess_value(Statistics->transport.lbtrm.naks_rcved),
					 preprocess_value(Statistics->transport.lbtrm.naks_ignored),
					 preprocess_value(Statistics->transport.lbtrm.naks_shed),
					 preprocess_value(Statistics->transport.lbtrm.naks_rx_delay_ignored),
					 preprocess_value(Statistics->transport.lbtrm.rxs_sent),
					 preprocess_value(Statistics->transport.lbtrm.rctlr_data_msgs),
					 preprocess_value(Statistics->transport.lbtrm.rctlr_rx_msgs),
					 preprocess_value(Statistics->transport.lbtrm.rx_bytes_sent));
			break;

		case LBM_TRANSPORT_STAT_LBTRU:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtru.msgs_sent),
					 preprocess_value(Statistics->transport.lbtru.bytes_sent),
					 preprocess_value(Statistics->transport.lbtru.nak_pckts_rcved),
					 preprocess_value(Statistics->transport.lbtru.naks_rcved),
					 preprocess_value(Statistics->transport.lbtru.naks_ignored),
					 preprocess_value(Statistics->transport.lbtru.naks_shed),
					 preprocess_value(Statistics->transport.lbtru.naks_rx_delay_ignored),
					 preprocess_value(Statistics->transport.lbtru.rxs_sent),
					 preprocess_value(Statistics->transport.lbtru.num_clients),
					 preprocess_value(Statistics->transport.lbtru.rx_bytes_sent));
			break;

		case LBM_TRANSPORT_STAT_LBTIPC:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtipc.num_clients),
					 preprocess_value(Statistics->transport.lbtipc.msgs_sent),
					 preprocess_value(Statistics->transport.lbtipc.bytes_sent));
			break;

		case LBM_TRANSPORT_STAT_LBTSMX:
			snprintf(ptr,
					remaining_len,
					"%lu,%lu,%lu\n",
					preprocess_value(Statistics->transport.lbtsmx.num_clients),
					preprocess_value(Statistics->transport.lbtsmx.msgs_sent),
					preprocess_value(Statistics->transport.lbtsmx.bytes_sent));
			break;

		case LBM_TRANSPORT_STAT_LBTRDMA:
			snprintf(ptr,
					 remaining_len,
					 "%lu,%lu,%lu\n",
					 preprocess_value(Statistics->transport.lbtrdma.num_clients),
					 preprocess_value(Statistics->transport.lbtrdma.msgs_sent),
					 preprocess_value(Statistics->transport.lbtrdma.bytes_sent));
			break;
	}
	write_stats(buffer, LBM_SRC_WRITE);
}

void evq_statistics_cb(const void * AttributeBlock, const lbm_event_queue_stats_t * Statistics, void * ClientData)
{
	unsigned int format;
	char buffer[4096];
	char * ptr;
	size_t remaining_len;

	memset(buffer, 0, sizeof(buffer));
	format = LBMMONUDP_EVQ_VER;
	++MessagesReceived;
	format_common_data(buffer, sizeof(buffer), format, AttributeBlock, NULL);
	ptr = &(buffer[strlen(buffer)]);
	remaining_len = sizeof(buffer) - strlen(buffer);
	snprintf(ptr,
			 remaining_len,
			 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
			 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
			 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
			 "%lu,%lu,%lu,%lu\n",
			 preprocess_value(Statistics->data_msgs),
			 preprocess_value(Statistics->data_msgs_tot),
			 preprocess_value(Statistics->data_msgs_svc_min),
			 preprocess_value(Statistics->data_msgs_svc_mean),
			 preprocess_value(Statistics->data_msgs_svc_max),
			 preprocess_value(Statistics->resp_msgs),
			 preprocess_value(Statistics->resp_msgs_tot),
			 preprocess_value(Statistics->resp_msgs_svc_min),
			 preprocess_value(Statistics->resp_msgs_svc_mean),
			 preprocess_value(Statistics->resp_msgs_svc_max),
			 preprocess_value(Statistics->topicless_im_msgs),
			 preprocess_value(Statistics->topicless_im_msgs_tot),
			 preprocess_value(Statistics->topicless_im_msgs_svc_min),
			 preprocess_value(Statistics->topicless_im_msgs_svc_mean),
			 preprocess_value(Statistics->topicless_im_msgs_svc_max),
			 preprocess_value(Statistics->wrcv_msgs),
			 preprocess_value(Statistics->wrcv_msgs_tot),
			 preprocess_value(Statistics->wrcv_msgs_svc_min),
			 preprocess_value(Statistics->wrcv_msgs_svc_mean),
			 preprocess_value(Statistics->wrcv_msgs_svc_max),
			 preprocess_value(Statistics->io_events),
			 preprocess_value(Statistics->io_events_tot),
			 preprocess_value(Statistics->io_events_svc_min),
			 preprocess_value(Statistics->io_events_svc_mean),
			 preprocess_value(Statistics->io_events_svc_max),
			 preprocess_value(Statistics->timer_events),
			 preprocess_value(Statistics->timer_events_tot),
			 preprocess_value(Statistics->timer_events_svc_min),
			 preprocess_value(Statistics->timer_events_svc_mean),
			 preprocess_value(Statistics->timer_events_svc_max),
			 preprocess_value(Statistics->source_events),
			 preprocess_value(Statistics->source_events_tot),
			 preprocess_value(Statistics->source_events_svc_min),
			 preprocess_value(Statistics->source_events_svc_mean),
			 preprocess_value(Statistics->source_events_svc_max),
			 preprocess_value(Statistics->unblock_events),
			 preprocess_value(Statistics->unblock_events_tot),
			 preprocess_value(Statistics->cancel_events),
			 preprocess_value(Statistics->cancel_events_tot),
			 preprocess_value(Statistics->cancel_events_svc_min),
			 preprocess_value(Statistics->cancel_events_svc_mean),
			 preprocess_value(Statistics->cancel_events_svc_max),
			 preprocess_value(Statistics->context_source_events),
			 preprocess_value(Statistics->context_source_events_tot),
			 preprocess_value(Statistics->context_source_events_svc_min),
			 preprocess_value(Statistics->context_source_events_svc_mean),
			 preprocess_value(Statistics->context_source_events_svc_max),
			 preprocess_value(Statistics->events),
			 preprocess_value(Statistics->events_tot),
			 preprocess_value(Statistics->age_min),
			 preprocess_value(Statistics->age_mean),
			 preprocess_value(Statistics->age_max));
	write_stats(buffer, LBM_EVQ_WRITE);
}

void ctx_statistics_cb(const void * AttributeBlock, const lbm_context_stats_t * Statistics, void * ClientData)
{
	unsigned int format;
	char buffer[4096];
	char * ptr;
	size_t remaining_len;

	memset(buffer, 0, sizeof(buffer));
	format = LBMMONUDP_CTX_VER;
	++MessagesReceived;
	format_common_data(buffer, sizeof(buffer), format, AttributeBlock, NULL);
	ptr = &(buffer[strlen(buffer)]);
	remaining_len = sizeof(buffer) - strlen(buffer);
	snprintf(ptr,
			 remaining_len,
			 "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
			 preprocess_value(Statistics->tr_dgrams_sent),
			 preprocess_value(Statistics->tr_bytes_sent),
			 preprocess_value(Statistics->tr_dgrams_rcved),
			 preprocess_value(Statistics->tr_bytes_rcved),
			 preprocess_value(Statistics->tr_dgrams_dropped_ver),
			 preprocess_value(Statistics->tr_dgrams_dropped_type),
			 preprocess_value(Statistics->tr_dgrams_dropped_malformed),
			 preprocess_value(Statistics->tr_dgrams_send_failed),
			 preprocess_value(Statistics->tr_src_topics),
			 preprocess_value(Statistics->tr_rcv_topics),
			 preprocess_value(Statistics->tr_rcv_unresolved_topics),
			 preprocess_value(Statistics->lbtrm_unknown_msgs_rcved),
			 preprocess_value(Statistics->lbtru_unknown_msgs_rcved),
			 preprocess_value(Statistics->send_blocked),
			 preprocess_value(Statistics->send_would_block),
			 preprocess_value(Statistics->resp_blocked),
			 preprocess_value(Statistics->resp_would_block),
			 preprocess_value(Statistics->uim_dup_msgs_rcved),
			 preprocess_value(Statistics->uim_msgs_no_stream_rcved));
	write_stats(buffer, LBM_CTX_WRITE);
}

int main(int argc, char * * argv)
{
	lbmmon_rctl_t * monctl;
	lbmmon_rctl_attr_t * attr;
	lbmmon_rcv_statistics_func_t rcv = { rcv_statistics_cb };
	lbmmon_src_statistics_func_t src = { src_statistics_cb };
	lbmmon_evq_statistics_func_t evq = { evq_statistics_cb };
	lbmmon_ctx_statistics_func_t ctx = { ctx_statistics_cb };
	int rc, i, c, errflag = 0;
	char * transport_options = NULL;
	char transport_options_string[1024];
	char * format_options = NULL;
	char format_options_string[1024];
	const lbmmon_transport_func_t * transport = lbmmon_transport_lbm_module();
	const lbmmon_format_func_t * format = lbmmon_format_csv_module();
	char *file, *ctx_file, *rcv_file, *src_file, *evq_file;
	int fn_len = 0;
	int header_opt = 0;

#ifdef _WIN32
	{
		WSADATA wsadata;
		int status;
		
		/* Windows socket setup code */
		if ((status = WSAStartup(MAKEWORD(2,2), &wsadata)) != 0)
		{
			fprintf(stderr, "%s: WSA startup error - %d\n", argv[0], status);
			exit(1);
		}
	}
#else
	/*
	 * Ignore SIGPIPE on UNIXes which can occur when writing to a socket
	 * with only one open end point.
	 */
	signal(SIGPIPE, SIG_IGN);
#endif /* _WIN32 */

	memset(transport_options_string, 0, sizeof(transport_options_string));
	memset(format_options_string, 0, sizeof(format_options_string));
	
        /* Print header */
        printf("*********************************************************************\n");
        printf("*\n");
        printf("*      UMTools\n");
        printf("*      umsub - Version %s\n", UM_MON_VERSION);
        printf("*      %s\n", lbm_version());
        print_platform_info();
        printf("*\n");
        printf("*      Parameters: ");
        for (i = 0; i < argc; i++)
                printf("%s ", argv[i]);
        printf("\n*\n");
        printf("*********************************************************************\n");


	while ((c = getopt_long(argc, argv, OptionString, OptionTable, NULL)) != EOF)
	{
		switch (c)
		{
			case 't':
				if (optarg != NULL)
				{
					if (strcasecmp(optarg, "lbm") == 0)
					{
						transport = lbmmon_transport_lbm_module();
					}
					else if (strcasecmp(optarg, "udp") == 0)
					{
						transport = lbmmon_transport_udp_module();						
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
					strncpy(transport_options_string, optarg, sizeof(transport_options_string));
				}
				else
				{
					++errflag;
				}
				break;
			case 'f':
				if (optarg != NULL)
				{
					if (strcasecmp(optarg, "csv") == 0)
					{
						format = lbmmon_format_csv_module();
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
					strncpy(format_options_string, optarg, sizeof(format_options_string));
				}
				else
				{
					++errflag;
				}
				break;
			case 'u':
				header_opt = atoi(optarg);
				break;
			case 'h':
				fprintf(stderr, "%s\n%s\n", lbm_version(), Purpose);
				fprintf(stderr, Usage, argv[0]);
				exit(0);
			default:
				errflag++;
				break;
		}
	}

	if (errflag != 0 || (optind == argc) )
	{
		fprintf(stderr, "%s\n", lbm_version());
		fprintf(stderr, Usage, argv[0]);
		exit(1);
	}

	file = argv[optind];
	fn_len = strlen(file);
	ctx_file = malloc(fn_len + 8);
	rcv_file = malloc(fn_len + 8);
	src_file = malloc(fn_len + 8);
	evq_file = malloc(fn_len + 8);

	sprintf(ctx_file, "%s.ctx.csv", file);
	sprintf(rcv_file, "%s.rcv.csv", file);
	sprintf(src_file, "%s.src.csv", file);
	sprintf(evq_file, "%s.evq.csv", file);	

	fp_ctx = fopen(ctx_file, "a");
	if (fp_ctx == NULL)
	{
		fprintf(stderr, "Failed to open %s. errno = %d\n", ctx_file, errno);
		exit(1);
	}
	fprintf(fp_ctx, LBM_CTX_HEADER);

	fp_rcv = fopen(rcv_file, "a");
        if (fp_rcv == NULL)
        {
                fprintf(stderr, "Failed to open %s. errno = %d\n", rcv_file, errno);
                exit(1);
        }
	write_rcv_header(header_opt);

	fp_src = fopen(src_file, "a");
        if (fp_src == NULL)
        {
                fprintf(stderr, "Failed to open %s. errno = %d\n", src_file, errno);
                exit(1);
        }
	write_src_header(header_opt);

	fp_evq = fopen(evq_file, "a");
        if (fp_evq == NULL)
        {
                fprintf(stderr, "Failed to open %s. errno = %d\n", evq_file, errno);
                exit(1);
        }
	fprintf(fp_evq, LBM_EVQ_HEADER);

	/* Setup the monitor receiver */
	if (strlen(transport_options_string) > 0)
	{
		transport_options = transport_options_string;
	}
	if (strlen(format_options_string) > 0)
	{
		format_options = format_options_string;
	}

	rc = lbmmon_rctl_attr_create(&attr);
	if (attr == NULL)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_create() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_RECEIVER_CALLBACK, (void *) &rcv, sizeof(rcv));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_SOURCE_CALLBACK, (void *) &src, sizeof(src));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_EVENT_QUEUE_CALLBACK, (void *) &evq, sizeof(evq));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_CONTEXT_CALLBACK, (void *) &ctx, sizeof(ctx));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}

	rc = lbmmon_rctl_create(&monctl, format, format_options, transport, transport_options, attr, NULL);
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_create() failed\n");
		exit(1);
	}
	lbmmon_rctl_attr_delete(attr);

	signal(SIGINT, SignalHandler);
	while (Terminate == 0)
	{
		SLEEP_SEC(2);
		printf("Stat msgs received %lu\n", MessagesReceived);
	}

	lbmmon_rctl_destroy(monctl);
	fclose(fp_ctx);
	fclose(fp_rcv);
	fclose(fp_src);
	fclose(fp_evq);

	return (0);
}

