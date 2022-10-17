/*
  lbmmon.c: example LBM monitoring application.

  (C) Copyright 2005,2022 Informatica LLC  Permission is granted to licensees to use
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
#ifdef _WIN32
	#include <winsock2.h>
	#define strcasecmp stricmp
#else
	#include <unistd.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <signal.h>
	#if defined(__TANDEM)
		#include <strings.h>
	#endif
#endif
#include "replgetopt.h"
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include "monmodopts.h"
#include "lbm/tnwgdmonmsgs.h"

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

int numtabs = 0;
const char *tabs[11] = {
"",
"\t",
"\t\t",
"\t\t\t",
"\t\t\t\t",
"\t\t\t\t\t",
"\t\t\t\t\t\t",
"\t\t\t\t\t\t\t",
"\t\t\t\t\t\t\t\t",
"\t\t\t\t\t\t\t\t\t",
"\t\t\t\t\t\t\t\t\t\t"
};
#define INCT() numtabs++
#define DECT() (numtabs != 0) ? numtabs-- : numtabs
#define PRTT() (numtabs > 10) ? "" : tabs[numtabs]

/* Lines starting with double quote are extracted for UM documentation. */

const char purpose[] = "Purpose: "
"example LBM statistics monitoring application."
;

const char usage[] =
"Usage: lbmmon [options]\n"
"Available options:\n"
"  -c, --config=FILE          Use LBM configuration file FILE.\n"
"                             Multiple config files are allowed.\n"
"                             Example:  '-c file1.cfg -c file2.cfg'\n"
"  -h, --help                 display this help and exit\n"
"  -t, --transport=TRANS      use transport module TRANS\n"
"                             TRANS may be `lbm', `udp', or `lbmsnmp', default is `lbm'\n"
"      --transport-opts=OPTS  use OPTS as transport module options\n"
"                             See the 'UM Operations Guide' section 'Monitoring Transport Modules'.\n"
"  -f, --format=FMT           use format module FMT\n"
"                             FMT may be `csv' or `pb'\n"
"      --format-opts=OPTS     use OPTS as format module options\n"
"                             See the 'UM Operations Guide' section 'Monitoring Format Modules'.\n"
MONMODULEOPTS_RECEIVER;

const char * OptionString = "c:f:ht:";
const struct option OptionTable[] =
{
	{ "config", required_argument, NULL, 'c' },
	{ "help", no_argument, NULL, 'h' },
	{ "transport", required_argument, NULL, 't' },
	{ "transport-opts", required_argument, NULL, 0 },
	{ "format", required_argument, NULL, 'f' },
	{ "format-opts", required_argument, NULL, 1 },
	{ NULL, 0, NULL, 0 }
};

const lbmmon_format_func_t * csv_format = NULL;
void * csv_format_data;
const lbmmon_format_func_t * pb_format = NULL;
void * pb_format_data;

int log_callback(int Level, const char * Message, void * ClientData);

void dump_hex_data(const unsigned char * Data, size_t Length)
{
	static char hextable[16] =
		{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	unsigned char c;
	size_t idx = 0;

	while (idx < Length)
	{
		c = Data[idx];
		printf("%c", hextable[((c >> 4) & 0x0f)]);
		printf("%c", hextable[(c & 0x0f)]);
		idx++;
	}
}


void print_attributes(const char * Message, const void * AttributeBlock, int IncludeContextAndDomainID)
{
	struct in_addr addr;
	char appid[256];
	time_t timestamp;
	char * time_string;
	lbm_ulong_t objectid = 0;
	lbm_ulong_t processid = 0;
	lbm_uint8_t ctxinst[LBM_CONTEXT_INSTANCE_BLOCK_SZ];
	lbm_uint32_t domain_id;

	printf("\n%s", Message);
	if (lbmmon_attr_get_appsourceid(AttributeBlock, appid, sizeof(appid)) == 0)
	{
		printf(" from %s", appid);
	}
	if (lbmmon_attr_get_ipv4sender(AttributeBlock, (lbm_uint_t *) &(addr.s_addr)) == 0)
	{
		printf(" at %s", inet_ntoa(addr));
	}
	if (lbmmon_attr_get_processid(AttributeBlock, &(processid)) == 0)
	{
		printf(", process ID=%0lx", processid);
	}
	if (lbmmon_attr_get_objectid(AttributeBlock, &(objectid)) == 0)
	{
		printf(", object ID=%0lx", objectid);
	}
	if (IncludeContextAndDomainID) {
		if (lbmmon_attr_get_ctxinst(AttributeBlock, ctxinst, sizeof(ctxinst)) == 0)
		{
			printf(", context instance=");
			dump_hex_data((unsigned char *)ctxinst, sizeof(ctxinst));
		}
		if (lbmmon_attr_get_domainid(AttributeBlock, &domain_id) == 0)
		{
			printf(", domain ID=%u", domain_id);
		}
	}
	if (lbmmon_attr_get_timestamp(AttributeBlock, &timestamp) == 0)
	{
		time_string = ctime(&timestamp);
		printf(", sent %s", time_string);
		/* Reminder: ctime() returns a string terminated with a newline */
	}
	else
	{
		printf("\n");
	}
}

const char * translate_transport(int Type)
{
	switch (Type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			return ("TCP");

		case LBM_TRANSPORT_STAT_LBTRM:
			return ("LBT-RM");

		case LBM_TRANSPORT_STAT_LBTRU:
			return ("LBT-RU");

		case LBM_TRANSPORT_STAT_LBTIPC:
			return ("LBT-IPC");

		case LBM_TRANSPORT_STAT_LBTRDMA:
			return ("LBT-RDMA");

		case LBM_TRANSPORT_STAT_LBTSMX:
			return ("LBT-SMX");

		case LBM_TRANSPORT_STAT_BROKER:
			return ("BROKER");

		default:
			return ("Unknown");
	}
}

const char * translate_pattern_type(lbm_uint8_t Type)
{
	switch (Type)
	{
		case LBM_WILDCARD_RCV_PATTERN_TYPE_PCRE:
			return ("PCRE");
		case LBM_WILDCARD_RCV_PATTERN_TYPE_REGEX:
			return ("Regex");
		default:
			return ("Unknown");
	}
}

void rcv_statistics_cb(const void * AttributeBlock, const lbm_rcv_transport_stats_t * Statistics, void * ClientData)
{
	lbm_ulong_t source = LBMMON_ATTR_SOURCE_NORMAL;

	if (lbmmon_attr_get_source(AttributeBlock, &source) != 0)
	{
		source = LBMMON_ATTR_SOURCE_NORMAL;
	}
	switch (source)
	{
		case LBMMON_ATTR_SOURCE_IM:
			print_attributes("IM receiver statistics received", AttributeBlock, 1);
			break;
		default:
			print_attributes("Receiver statistics received", AttributeBlock, 1);
			break;
	}
	printf("Source: %s\n", Statistics->source);
	printf("Transport: %s\n", translate_transport(Statistics->type));
	switch (Statistics->type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			printf("\tLBT-TCP bytes received                                    : %lu\n", Statistics->transport.tcp.bytes_rcved);
			printf("\tLBM messages received                                     : %lu\n", Statistics->transport.tcp.lbm_msgs_rcved);
			printf("\tLBM messages received with uninteresting topic            : %lu\n", Statistics->transport.tcp.lbm_msgs_no_topic_rcved);
			printf("\tLBM requests received                                     : %lu\n", Statistics->transport.tcp.lbm_reqs_rcved);
			break;

		case LBM_TRANSPORT_STAT_LBTRM:
			printf("\tLBT-RM datagrams received                                 : %lu\n", Statistics->transport.lbtrm.msgs_rcved);
			printf("\tLBT-RM datagram bytes received                            : %lu\n", Statistics->transport.lbtrm.bytes_rcved);
			printf("\tLBT-RM NAK packets sent                                   : %lu\n", Statistics->transport.lbtrm.nak_pckts_sent);
			printf("\tLBT-RM NAKs sent                                          : %lu\n", Statistics->transport.lbtrm.naks_sent);
			printf("\tLost LBT-RM datagrams detected                            : %lu\n", Statistics->transport.lbtrm.lost);
			printf("\tNCFs received (ignored)                                   : %lu\n", Statistics->transport.lbtrm.ncfs_ignored);
			printf("\tNCFs received (shed)                                      : %lu\n", Statistics->transport.lbtrm.ncfs_shed);
			printf("\tNCFs received (retransmit delay)                          : %lu\n", Statistics->transport.lbtrm.ncfs_rx_delay);
			printf("\tNCFs received (unknown)                                   : %lu\n", Statistics->transport.lbtrm.ncfs_unknown);
			printf("\tLoss recovery minimum time                                : %lums\n", Statistics->transport.lbtrm.nak_stm_min);
			printf("\tLoss recovery mean time                                   : %lums\n", Statistics->transport.lbtrm.nak_stm_mean);
			printf("\tLoss recovery maximum time                                : %lums\n", Statistics->transport.lbtrm.nak_stm_max);
			printf("\tMinimum transmissions per individual NAK                  : %lu\n", Statistics->transport.lbtrm.nak_tx_min);
			printf("\tMean transmissions per individual NAK                     : %lu\n", Statistics->transport.lbtrm.nak_tx_mean);
			printf("\tMaximum transmissions per individual NAK                  : %lu\n", Statistics->transport.lbtrm.nak_tx_max);
			printf("\tDuplicate LBT-RM datagrams received                       : %lu\n", Statistics->transport.lbtrm.duplicate_data);
			printf("\tLBT-RM datagrams unrecoverable (window advance)           : %lu\n", Statistics->transport.lbtrm.unrecovered_txw);
			printf("\tLBT-RM datagrams unrecoverable (NAK generation expiration): %lu\n", Statistics->transport.lbtrm.unrecovered_tmo);
			printf("\tLBT-RM LBM messages received                              : %lu\n", Statistics->transport.lbtrm.lbm_msgs_rcved);
			printf("\tLBT-RM LBM messages received with uninteresting topic     : %lu\n", Statistics->transport.lbtrm.lbm_msgs_no_topic_rcved);
			printf("\tLBT-RM LBM requests received                              : %lu\n", Statistics->transport.lbtrm.lbm_reqs_rcved);
			printf("\tLBT-RM datagrams dropped (size)                           : %lu\n", Statistics->transport.lbtrm.dgrams_dropped_size);
			printf("\tLBT-RM datagrams dropped (type)                           : %lu\n", Statistics->transport.lbtrm.dgrams_dropped_type);
			printf("\tLBT-RM datagrams dropped (version)                        : %lu\n", Statistics->transport.lbtrm.dgrams_dropped_version);
			printf("\tLBT-RM datagrams dropped (hdr)                            : %lu\n", Statistics->transport.lbtrm.dgrams_dropped_hdr);
			printf("\tLBT-RM datagrams dropped (other)                          : %lu\n", Statistics->transport.lbtrm.dgrams_dropped_other);
			printf("\tLBT-RM datagrams received out of order                    : %lu\n", Statistics->transport.lbtrm.out_of_order);
			break;

		case LBM_TRANSPORT_STAT_LBTRU:
			printf("\tLBT-RU datagrams received                                 : %lu\n", Statistics->transport.lbtru.msgs_rcved);
			printf("\tLBT-RU datagram bytes received                            : %lu\n", Statistics->transport.lbtru.bytes_rcved);
			printf("\tLBT-RU NAK packets sent                                   : %lu\n", Statistics->transport.lbtru.nak_pckts_sent);
			printf("\tLBT-RU NAKs sent                                          : %lu\n", Statistics->transport.lbtru.naks_sent);
			printf("\tLost LBT-RU datagrams detected                            : %lu\n", Statistics->transport.lbtru.lost);
			printf("\tNCFs received (ignored)                                   : %lu\n", Statistics->transport.lbtru.ncfs_ignored);
			printf("\tNCFs received (shed)                                      : %lu\n", Statistics->transport.lbtru.ncfs_shed);
			printf("\tNCFs received (retransmit delay)                          : %lu\n", Statistics->transport.lbtru.ncfs_rx_delay);
			printf("\tNCFs received (unknown)                                   : %lu\n", Statistics->transport.lbtru.ncfs_unknown);
			printf("\tLoss recovery minimum time                                : %lums\n", Statistics->transport.lbtru.nak_stm_min);
			printf("\tLoss recovery mean time                                   : %lums\n", Statistics->transport.lbtru.nak_stm_mean);
			printf("\tLoss recovery maximum time                                : %lums\n", Statistics->transport.lbtru.nak_stm_max);
			printf("\tMinimum transmissions per individual NAK                  : %lu\n", Statistics->transport.lbtru.nak_tx_min);
			printf("\tMean transmissions per individual NAK                     : %lu\n", Statistics->transport.lbtru.nak_tx_mean);
			printf("\tMaximum transmissions per individual NAK                  : %lu\n", Statistics->transport.lbtru.nak_tx_max);
			printf("\tDuplicate LBT-RU datagrams received                       : %lu\n", Statistics->transport.lbtru.duplicate_data);
			printf("\tLBT-RU datagrams unrecoverable (window advance)           : %lu\n", Statistics->transport.lbtru.unrecovered_txw);
			printf("\tLBT-RU datagrams unrecoverable (NAK generation expiration): %lu\n", Statistics->transport.lbtru.unrecovered_tmo);
			printf("\tLBT-RU LBM messages received                              : %lu\n", Statistics->transport.lbtru.lbm_msgs_rcved);
			printf("\tLBT-RU LBM messages received with uninteresting topic     : %lu\n", Statistics->transport.lbtru.lbm_msgs_no_topic_rcved);
			printf("\tLBT-RU LBM requests received                              : %lu\n", Statistics->transport.lbtru.lbm_reqs_rcved);
			printf("\tLBT-RU datagrams dropped (size)                           : %lu\n", Statistics->transport.lbtru.dgrams_dropped_size);
			printf("\tLBT-RU datagrams dropped (type)                           : %lu\n", Statistics->transport.lbtru.dgrams_dropped_type);
			printf("\tLBT-RU datagrams dropped (version)                        : %lu\n", Statistics->transport.lbtru.dgrams_dropped_version);
			printf("\tLBT-RU datagrams dropped (hdr)                            : %lu\n", Statistics->transport.lbtru.dgrams_dropped_hdr);
			printf("\tLBT-RU datagrams dropped (SID)                            : %lu\n", Statistics->transport.lbtru.dgrams_dropped_sid);
			printf("\tLBT-RU datagrams dropped (other)                          : %lu\n", Statistics->transport.lbtru.dgrams_dropped_other);
			break;

		case LBM_TRANSPORT_STAT_LBTIPC:
			printf("\tLBT-IPC datagrams received                                : %lu\n", Statistics->transport.lbtipc.msgs_rcved);
			printf("\tLBT-IPC datagram bytes received                           : %lu\n", Statistics->transport.lbtipc.bytes_rcved);
			printf("\tLBT-IPC LBM messages received                             : %lu\n", Statistics->transport.lbtipc.lbm_msgs_rcved);
			printf("\tLBT-IPC LBM messages received with uninteresting topic    : %lu\n", Statistics->transport.lbtipc.lbm_msgs_no_topic_rcved);
			printf("\tLBT-IPC LBM requests received                             : %lu\n", Statistics->transport.lbtipc.lbm_reqs_rcved);
			break;

		case LBM_TRANSPORT_STAT_LBTSMX:
			printf("\tLBT-SMX datagrams received                                : %lu\n", Statistics->transport.lbtsmx.msgs_rcved);
			printf("\tLBT-SMX datagram bytes received                           : %lu\n", Statistics->transport.lbtsmx.bytes_rcved);
			printf("\tLBT-SMX LBM messages received                             : %lu\n", Statistics->transport.lbtsmx.lbm_msgs_rcved);
			printf("\tLBT-SMX LBM messages received with uninteresting topic    : %lu\n", Statistics->transport.lbtsmx.lbm_msgs_no_topic_rcved);
			break;

		case LBM_TRANSPORT_STAT_LBTRDMA:
			printf("\tLBT-RDMA datagrams received              : %lu\n", Statistics->transport.lbtrdma.msgs_rcved);
			printf("\tLBT-RDMA datagram bytes received         : %lu\n", Statistics->transport.lbtrdma.bytes_rcved);
			printf("\tLBT-RDMA LBM messages received           : %lu\n", Statistics->transport.lbtrdma.lbm_msgs_rcved);
			printf("\tLBT-RDMA LBM messages received (no topic): %lu\n", Statistics->transport.lbtrdma.lbm_msgs_no_topic_rcved);
			printf("\tLBT-RDMA LBM requests received           : %lu\n", Statistics->transport.lbtrdma.lbm_reqs_rcved);
			break;

		case LBM_TRANSPORT_STAT_BROKER:
			printf("\tBROKER messages received              : %lu\n", Statistics->transport.broker.msgs_rcved);
			printf("\tBROKER bytes received                 : %lu\n", Statistics->transport.broker.bytes_rcved);
			break;
	}
	fflush(stdout);
}

void src_statistics_cb(const void * AttributeBlock, const lbm_src_transport_stats_t * Statistics, void * ClientData)
{
	lbm_ulong_t source = LBMMON_ATTR_SOURCE_NORMAL;

	if (lbmmon_attr_get_source(AttributeBlock, &source) != 0)
	{
		source = LBMMON_ATTR_SOURCE_NORMAL;
	}
	switch (source)
	{
		case LBMMON_ATTR_SOURCE_IM:
			print_attributes("IM source statistics received", AttributeBlock, 1);
			break;
		default:
			print_attributes("Source statistics received", AttributeBlock, 1);
			break;
	}
	printf("Source: %s\n", Statistics->source);
	printf("Transport: %s\n", translate_transport(Statistics->type));
	switch (Statistics->type)
	{
		case LBM_TRANSPORT_STAT_TCP:
			printf("\tClients       : %lu\n", Statistics->transport.tcp.num_clients);
			printf("\tBytes buffered: %lu\n", Statistics->transport.tcp.bytes_buffered);
			break;

		case LBM_TRANSPORT_STAT_LBTRM:
			printf("\tLBT-RM datagrams sent                                 : %lu\n", Statistics->transport.lbtrm.msgs_sent);
			printf("\tLBT-RM datagram bytes sent                            : %lu\n", Statistics->transport.lbtrm.bytes_sent);
			printf("\tLBT-RM datagrams in transmission window               : %lu\n", Statistics->transport.lbtrm.txw_msgs);
			printf("\tLBT-RM datagram bytes in transmission window          : %lu\n", Statistics->transport.lbtrm.txw_bytes);
			printf("\tLBT-RM NAK packets received                           : %lu\n", Statistics->transport.lbtrm.nak_pckts_rcved);
			printf("\tLBT-RM NAKs received                                  : %lu\n", Statistics->transport.lbtrm.naks_rcved);
			printf("\tLBT-RM NAKs ignored                                   : %lu\n", Statistics->transport.lbtrm.naks_ignored);
			printf("\tLBT-RM NAKs shed                                      : %lu\n", Statistics->transport.lbtrm.naks_shed);
			printf("\tLBT-RM NAKs ignored (retransmit delay)                : %lu\n", Statistics->transport.lbtrm.naks_rx_delay_ignored);
			printf("\tLBT-RM retransmission datagrams sent                  : %lu\n", Statistics->transport.lbtrm.rxs_sent);
			printf("\tLBT-RM datagrams queued by rate control               : %lu\n", Statistics->transport.lbtrm.rctlr_data_msgs);
			printf("\tLBT-RM retransmission datagrams queued by rate control: %lu\n", Statistics->transport.lbtrm.rctlr_rx_msgs);
			printf("\tLBT-RM retransmission datagram bytes sent             : %lu\n", Statistics->transport.lbtrm.rx_bytes_sent);
			break;

		case LBM_TRANSPORT_STAT_LBTRU:
			printf("\tLBT-RU datagrams sent                    : %lu\n", Statistics->transport.lbtru.msgs_sent);
			printf("\tLBT-RU datagram bytes sent               : %lu\n", Statistics->transport.lbtru.bytes_sent);
			printf("\tLBT-RU NAK packets received              : %lu\n", Statistics->transport.lbtru.nak_pckts_rcved);
			printf("\tLBT-RU NAKs received                     : %lu\n", Statistics->transport.lbtru.naks_rcved);
			printf("\tLBT-RU NAKs ignored                      : %lu\n", Statistics->transport.lbtru.naks_ignored);
			printf("\tLBT-RU NAKs shed                         : %lu\n", Statistics->transport.lbtru.naks_shed);
			printf("\tLBT-RU NAKs ignored (retransmit delay)   : %lu\n", Statistics->transport.lbtru.naks_rx_delay_ignored);
			printf("\tLBT-RU retransmission datagrams sent     : %lu\n", Statistics->transport.lbtru.rxs_sent);
			printf("\tClients                                  : %lu\n", Statistics->transport.lbtru.num_clients);
			printf("\tLBT-RU retransmission datagram bytes sent: %lu\n", Statistics->transport.lbtru.rx_bytes_sent);
			break;

		case LBM_TRANSPORT_STAT_LBTIPC:
			printf("\tClients                    : %lu\n", Statistics->transport.lbtipc.num_clients);
			printf("\tLBT-IPC datagrams sent     : %lu\n", Statistics->transport.lbtipc.msgs_sent);
			printf("\tLBT-IPC datagram bytes sent: %lu\n", Statistics->transport.lbtipc.bytes_sent);
			break;

		case LBM_TRANSPORT_STAT_LBTSMX:
			printf("\tClients                   : %lu\n", Statistics->transport.lbtsmx.num_clients);
			printf("\tLBT-SMX datagrams sent     : %lu\n", Statistics->transport.lbtsmx.msgs_sent);
			printf("\tLBT-SMX datagram bytes sent: %lu\n", Statistics->transport.lbtsmx.bytes_sent);
			break;

		case LBM_TRANSPORT_STAT_LBTRDMA:
			printf("\tClients                    : %lu\n", Statistics->transport.lbtrdma.num_clients);
			printf("\tLBT-RDMA datagrams sent     : %lu\n", Statistics->transport.lbtrdma.msgs_sent);
			printf("\tLBT-RDMA datagram bytes sent: %lu\n", Statistics->transport.lbtrdma.bytes_sent);
			break;

		case LBM_TRANSPORT_STAT_BROKER:
			printf("\tBROKER messages sent               : %lu\n", Statistics->transport.broker.msgs_sent);
			printf("\tBROKER bytes sent                  : %lu\n", Statistics->transport.broker.bytes_sent);
			break;
	}
	fflush(stdout);
}

void evq_statistics_cb(const void * AttributeBlock, const lbm_event_queue_stats_t * Statistics, void * ClientData)
{
	print_attributes("Event queue statistics received", AttributeBlock, 0);
	printf("\tData messages currently enqueued                : %lu\n", Statistics->data_msgs);
	printf("\tTotal data messages enqueued                    : %lu\n", Statistics->data_msgs_tot);
	printf("\tData message service minimum time               : %luus\n", Statistics->data_msgs_svc_min);
	printf("\tData message service mean time                  : %luus\n", Statistics->data_msgs_svc_mean);
	printf("\tData message service maximum time               : %luus\n", Statistics->data_msgs_svc_max);
	printf("\tResponses currently enqueued                    : %lu\n", Statistics->resp_msgs);
	printf("\tTotal responses enqueued                        : %lu\n", Statistics->resp_msgs_tot);
	printf("\tResponse service minimum time                   : %luus\n", Statistics->resp_msgs_svc_min);
	printf("\tResponse service mean time                      : %luus\n", Statistics->resp_msgs_svc_mean);
	printf("\tResponse service maximum time                   : %luus\n", Statistics->resp_msgs_svc_max);
	printf("\tTopicless immediate messages currently enqueued : %lu\n", Statistics->topicless_im_msgs);
	printf("\tTotal topicless immediate messages enqueued     : %lu\n", Statistics->topicless_im_msgs_tot);
	printf("\tTopicless immediate message service minimum time: %luus\n", Statistics->topicless_im_msgs_svc_min);
	printf("\tTopicless immediate message service mean time   : %luus\n", Statistics->topicless_im_msgs_svc_mean);
	printf("\tTopicless immediate message service maximum time: %luus\n", Statistics->topicless_im_msgs_svc_max);
	printf("\tWildcard messages currently enqueued            : %lu\n", Statistics->wrcv_msgs);
	printf("\tTotal wildcard messages enqueued                : %lu\n", Statistics->wrcv_msgs_tot);
	printf("\tWildcard message service minimum time           : %luus\n", Statistics->wrcv_msgs_svc_min);
	printf("\tWildcard message service mean time              : %luus\n", Statistics->wrcv_msgs_svc_mean);
	printf("\tWildcard message service maximum time           : %luus\n", Statistics->wrcv_msgs_svc_max);
	printf("\tI/O events currently enqueued                   : %lu\n", Statistics->io_events);
	printf("\tTotal I/O events enqueued                       : %lu\n", Statistics->io_events_tot);
	printf("\tI/O event service minimum time                  : %luus\n", Statistics->io_events_svc_min);
	printf("\tI/O event service mean time                     : %luus\n", Statistics->io_events_svc_mean);
	printf("\tI/O event service maximum time                  : %luus\n", Statistics->io_events_svc_max);
	printf("\tTimer events currently enqueued                 : %lu\n", Statistics->timer_events);
	printf("\tTotal timer events enqueued                     : %lu\n", Statistics->timer_events_tot);
	printf("\tTimer event service minimum time                : %luus\n", Statistics->timer_events_svc_min);
	printf("\tTimer event service mean time                   : %luus\n", Statistics->timer_events_svc_mean);
	printf("\tTimer event service maximum time                : %luus\n", Statistics->timer_events_svc_max);
	printf("\tSource events currently enqueued                : %lu\n", Statistics->source_events);
	printf("\tTotal source events enqueued                    : %lu\n", Statistics->source_events_tot);
	printf("\tSource event service minimum time               : %luus\n", Statistics->source_events_svc_min);
	printf("\tSource event service mean time                  : %luus\n", Statistics->source_events_svc_mean);
	printf("\tSource event service maximum time               : %luus\n", Statistics->source_events_svc_max);
	printf("\tUnblock events currently enqueued               : %lu\n", Statistics->unblock_events);
	printf("\tTotal unblock events enqueued                   : %lu\n", Statistics->unblock_events_tot);
	printf("\tCancel events currently enqueued                : %lu\n", Statistics->cancel_events);
	printf("\tTotal cancel events enqueued                    : %lu\n", Statistics->cancel_events_tot);
	printf("\tCancel event service minimum time               : %luus\n", Statistics->cancel_events_svc_min);
	printf("\tCancel event service mean time                  : %luus\n", Statistics->cancel_events_svc_mean);
	printf("\tCancel event service maximum time               : %luus\n", Statistics->cancel_events_svc_max);
	printf("\tCallback events currently enqueued              : %lu\n", Statistics->callback_events);
	printf("\tTotal callback events enqueued                  : %lu\n", Statistics->callback_events_tot);
	printf("\tCallback event service minimum time             : %luus\n", Statistics->callback_events_svc_min);
	printf("\tCallback event service mean time                : %luus\n", Statistics->callback_events_svc_mean);
	printf("\tCallback event service maximum time             : %luus\n", Statistics->callback_events_svc_max);
	printf("\tContext source events currently enqueued        : %lu\n", Statistics->context_source_events);
	printf("\tTotal context source events enqueued            : %lu\n", Statistics->context_source_events_tot);
	printf("\tContext source event service minimum time       : %luus\n", Statistics->context_source_events_svc_min);
	printf("\tContext source event service mean time          : %luus\n", Statistics->context_source_events_svc_mean);
	printf("\tContext source event service maximum time       : %luus\n", Statistics->context_source_events_svc_max);
	printf("\tEvents currently enqueued                       : %lu\n", Statistics->events);
	printf("\tTotal events enqueued                           : %lu\n", Statistics->events_tot);
	printf("\tEvent latency minimum time                      : %luus\n", Statistics->age_min);
	printf("\tEvent latency mean time                         : %luus\n", Statistics->age_mean);
	printf("\tEvent latency maximum time                      : %luus\n", Statistics->age_max);
	fflush(stdout);
}

void ctx_statistics_cb(const void * AttributeBlock, const lbm_context_stats_t * Statistics, void * ClientData)
{
	print_attributes("Context statistics received", AttributeBlock, 1);
	printf("\tTopic resolution datagrams sent                    : %lu\n", Statistics->tr_dgrams_sent);
	printf("\tTopic resolution datagram bytes sent               : %lu\n", Statistics->tr_bytes_sent);
	printf("\tTopic resolution datagrams received                : %lu\n", Statistics->tr_dgrams_rcved);
	printf("\tTopic resolution datagram bytes received           : %lu\n", Statistics->tr_bytes_rcved);
	printf("\tTopic resolution datagrams dropped (version)       : %lu\n", Statistics->tr_dgrams_dropped_ver);
	printf("\tTopic resolution datagrams dropped (type)          : %lu\n", Statistics->tr_dgrams_dropped_type);
	printf("\tTopic resolution datagrams dropped (malformed)     : %lu\n", Statistics->tr_dgrams_dropped_malformed);
	printf("\tTopic resolution send failures                     : %lu\n", Statistics->tr_dgrams_send_failed);
	printf("\tTopics in source topic map                         : %lu\n", Statistics->tr_src_topics);
	printf("\tTopics in receiver topic map                       : %lu\n", Statistics->tr_rcv_topics);
	printf("\tUnresolved topics in receiver topic map            : %lu\n", Statistics->tr_rcv_unresolved_topics);
	printf("\tUnknown LBT-RM datagrams received                  : %lu\n", Statistics->lbtrm_unknown_msgs_rcved);
	printf("\tUnknown LBT-RU datagrams received                  : %lu\n", Statistics->lbtru_unknown_msgs_rcved);
	printf("\tNumber of times message send blocked               : %lu\n", Statistics->send_blocked);
	printf("\tNumber of times message send returned EWOULDBLOCK  : %lu\n", Statistics->send_would_block);
	printf("\tNumber of times response send blocked              : %lu\n", Statistics->resp_blocked);
	printf("\tNumber of times response send returned EWOULDBLOCK : %lu\n", Statistics->resp_would_block);
	printf("\tNumber of duplicate UIM messages dropped           : %lu\n", Statistics->uim_dup_msgs_rcved);
	printf("\tNumber of UIM messages received without stream info: %lu\n", Statistics->uim_msgs_no_stream_rcved);
	printf("\tNumber of data message fragments lost              : %lu\n", Statistics->fragments_lost);
	printf("\tNumber of data message fragments unrecoverably lost: %lu\n", Statistics->fragments_unrecoverably_lost);
	printf("\tReceiver callbacks min service time (microseconds) : %lu\n", Statistics->rcv_cb_svc_time_min);
	printf("\tReceiver callbacks max service time (microseconds) : %lu\n", Statistics->rcv_cb_svc_time_max);
	printf("\tReceiver callbacks mean service time (microseconds): %lu\n", Statistics->rcv_cb_svc_time_mean);
	fflush(stdout);
}

void rcv_topic_statistics_cb(const void * AttributeBlock, const lbm_rcv_topic_stats_t * Statistics, void * ClientData)
{
	char *formatted_time;
	print_attributes("Receiver topic statistics received", AttributeBlock, 1);
	printf("\tTopic                                              : %s\n", Statistics->topic);
	if (Statistics->flags == LBM_RCV_TOPIC_STATS_FLAG_NO_SOURCE)
	{
		printf("\tNo known sources\n");
	} else {
		printf("\tSource                                             : %s\n", Statistics->source);
		printf("\tOTID                                               : ");
		dump_hex_data(Statistics->otid, LBM_OTID_BLOCK_SZ);
		printf("\n");
		printf("\tTopic index                                        : %u\n", Statistics->topic_idx);
		if (Statistics->timestamp_sec != 0) {
			formatted_time = ctime((time_t *)&Statistics->timestamp_sec);
			formatted_time[strlen(formatted_time) - 1] = '\0';
			printf("\tSource state                                       : %s on %s (%ld usec)\n",
				Statistics->flags == LBM_RCV_TOPIC_STATS_FLAG_SRC_CREATED ? "Created" : "Deleted",
				formatted_time, Statistics->timestamp_usec);
		}
	}
	fflush(stdout);
}

void wildcard_receiver_statistics_cb(const void * AttributeBlock, const lbm_wildcard_rcv_stats_t * Statistics, void * ClientData)
{
	print_attributes("Wildcard receiver statistics received", AttributeBlock, 1);
	printf("\tWildcard receiver pattern   : %s\n", Statistics->pattern);
	printf("\tWildcard receiver type      : 0x%0x\n", Statistics->type);
	fflush(stdout);
}

void umestore_statistics_cb(const void * AttributeBlock, const Lbmmon__UMPMonMsg * store_lbmmon_msg, void * ClientData)
{
	if (store_lbmmon_msg->stats != NULL) {
		print_attributes("Store statistics received", AttributeBlock, 1);
		if (store_lbmmon_msg->stats != NULL) {
			Lbmmon__UMPMonMsg__Stats *stats = store_lbmmon_msg->stats;
			printf("\tStore statistics:\n");
			printf("\tStore index                          : %u\n", stats->store_idx);
			printf("\tRetransmission request receive count : %u\n", stats->ume_retx_req_rcv_count);
			printf("\tRetransmission request serviced count: %u\n", stats->ume_retx_req_serviced_count);
			printf("\tRetransmission request drop count    : %u\n", stats->ume_retx_req_drop_count);
			printf("\tRetransmission request total dropped : %u\n", stats->ume_retx_req_total_dropped);
			printf("\tRetransmission statistics interval   : %u\n", stats->ume_retx_stat_interval);
			if (store_lbmmon_msg->stats->smart_heap_stat != NULL) {
				Lbmmon__UMPMonMsg__Stats__SmartHeapStat *smart_heap_stat = store_lbmmon_msg->stats->smart_heap_stat;
				printf("\tSmart Heap statistics:\n");
				printf("\t\tPage size       : %lu\n", smart_heap_stat->pagesize);
				printf("\t\tPool count      : %lu\n", smart_heap_stat->poolcount);
				printf("\t\tPool size       : %lu\n", smart_heap_stat->poolsize);
				printf("\t\tSmall block size: %lu\n", smart_heap_stat->smallblocksize);
			}
			if (store_lbmmon_msg->stats->n_src_repo_stats > 0) {
				int src_repo_idx;
				for (src_repo_idx = 0; src_repo_idx < store_lbmmon_msg->stats->n_src_repo_stats; src_repo_idx++) {
					Lbmmon__UMPMonMsg__Stats__SrcRepoStat *src_repo_stat = store_lbmmon_msg->stats->src_repo_stats[src_repo_idx];
					printf("\tRepository statistics:\n");
					printf("\t\tLast activity timestamp     : %lu\n", src_repo_stat->last_activity_timestamp_sec);
					printf("\t\tSource registration ID      : %u\n", src_repo_stat->src_regid);
					printf("\t\tTopic name                  : %s\n", src_repo_stat->topic_name);
					printf("\t\tSession ID                  : %lu\n", src_repo_stat->src_session_id);
					printf("\t\tContiguous sqn              : %u\n", src_repo_stat->contig_sqn);
					printf("\t\tSynchronization complete sqn: %u\n", src_repo_stat->sync_complete_sqn);
					printf("\t\tTrail sqn                   : %u\n", src_repo_stat->trail_sqn);
					printf("\t\tFlags                       : %u\n", src_repo_stat->flags);
					printf("\t\tHigh ULB sqn                : %u\n", src_repo_stat->high_ulb_sqn);
					printf("\t\tSize limit drops            : %lu\n", src_repo_stat->sz_limit_drops);
					printf("\t\tMap intentional drops       : %u\n", src_repo_stat->map_intentional_drops);
					printf("\t\tMemory size                 : %lu\n", src_repo_stat->memory_sz);
					printf("\t\tMemory trail sqn            : %u\n", src_repo_stat->mem_trail_sqn);
					printf("\t\tMessage map size            : %lu\n", src_repo_stat->message_map_sz);
					printf("\t\tRPP memory size             : %lu\n", src_repo_stat->rpp_memory_sz);
					printf("\t\tULB count                   : %lu\n", src_repo_stat->ulbs);
					printf("\t\tUL count                    : %lu\n", src_repo_stat->uls);
					printf("\t\tReceiver count              : %u\n", src_repo_stat->rcvr_count);
					if (src_repo_stat->src_disk_stat != NULL) {
						Lbmmon__UMPMonMsg__Stats__SrcRepoStat__SrcDiskStat *src_disk_stat = src_repo_stat->src_disk_stat;
						printf("\tDisk statistics:\n");
						printf("\t\tStarting offset           : %lu\n", src_disk_stat->start_offset);
						printf("\t\tCurrent offset            : %lu\n", src_disk_stat->offset);
						printf("\t\tMaximum offset            : %lu\n", src_disk_stat->max_offset);
						printf("\t\tNumber of IOs pending     : %lu\n", src_disk_stat->num_ios_pending);
						printf("\t\tNumber of read IOs pending: %lu\n", src_disk_stat->num_read_ios_pending);
					}
					if (src_repo_stat->n_rcv_stats > 0) {
						int rcv_idx;
						for (rcv_idx = 0; rcv_idx < src_repo_stat->n_rcv_stats; ++rcv_idx) {
							Lbmmon__UMPMonMsg__Stats__SrcRepoStat__RcvStat * rcv_stats = src_repo_stat->rcv_stats[rcv_idx];
							printf("\tReceiver %d statistics:\n", rcv_idx);
							printf("\t\tLast activity timestamp : %lu\n", rcv_stats->last_activity_timestamp_sec);
							printf("\t\tReceiver registration ID: %u\n", rcv_stats->rcv_regid);
							printf("\t\tHigh ACK Sequence Number: %u\n", rcv_stats->high_ack_sqn);
							printf("\t\tFlags                   : %u\n", rcv_stats->flags);
							printf("\t\tSession ID              : %lu\n", rcv_stats->rcv_session_id);
						}
					}
				}
			}

		}
	}
	if (store_lbmmon_msg->configs != NULL) {
		struct in_addr addr;
		Lbmmon__UMPMonMsg__Configs *configs = store_lbmmon_msg->configs;

		addr.s_addr = configs->ip_addr;
		print_attributes("Store configuration received", AttributeBlock, 1);
		printf("\tStore index                       : %u\n", configs->store_idx);
		printf("\tStore name                        : %s\n", configs->store_name);
		printf("\tLBM version                       : %s\n", configs->lbm_version);
		if (strlen(configs->smartheap_version) > 0) {
			printf("\tSmart heap version                : %s\n", configs->smartheap_version);
		}
		printf("\tContext ID                        : %u\n", configs->context_id);
		printf("\tDisk cache directory              : %s\n", configs->disk_cache_dir_name);
		printf("\tDisk state directory              : %s\n", configs->disk_state_dir_name);
		printf("\tIP address                        : %s\n", inet_ntoa(addr));
		printf("\tPort                              : %u\n", ntohs(configs->port));
		printf("\tMax retransmission Processing rate: %u\n", configs->max_retransmission_processing_rate);
		printf("\tNumber of sources                 : %u\n", configs->src_count);
		if (configs->n_pattern_configs > 0) {
			int pattern_idx;
			for (pattern_idx = 0; pattern_idx < configs->n_pattern_configs; ++pattern_idx) {
				Lbmmon__UMPMonMsg__Configs__PatternConfig *config = configs->pattern_configs[pattern_idx];
				printf("\tPattern configuration %d\n", pattern_idx);
				printf("\t\tPattern: %s\n", config->pattern);
				printf("\t\tType   : %u\n", config->topic_type);
			}
		}
		if (configs->n_topic_configs > 0) {
			int topic_idx;
			for (topic_idx = 0; topic_idx < configs->n_topic_configs; ++topic_idx) {
				Lbmmon__UMPMonMsg__Configs__TopicConfig *config = configs->topic_configs[topic_idx];
				printf("\tTopic configuration %d\n", topic_idx);
				printf("\t\tDmon topic index: %u\n", config->dmon_topic_idx);
				printf("\t\tTopic name      : %s\n", config->topic_name);
				if (config->n_repo_configs > 0) {
					int repo_idx;
					for (repo_idx = 0; repo_idx < config->n_repo_configs; ++repo_idx) {
						Lbmmon__UMPMonMsg__Configs__TopicConfig__RepoConfig *repo_config = config->repo_configs[repo_idx];
						printf("\tRepository configuration %d\n", repo_idx);
						printf("\t\tSource registration ID                : %u\n", repo_config->src_regid);
						printf("\t\tSource session ID                     : %lu\n", repo_config->src_session_id);
						printf("\t\tSource string                         : %s\n", repo_config->source_string);
						printf("\t\tSource domain ID                      : %u\n", repo_config->src_domain_id);
						printf("\t\tRepository type                       : %u\n", repo_config->repository_type);
						printf("\t\tAge threshold                         : %u\n", repo_config->age_threshold);
						printf("\t\tAllow ACK on reception                : %u\n", repo_config->allow_ack_on_reception);
						printf("\t\tDisk asynchronous IO Buffer length    : %u\n", repo_config->disk_aio_buffer_len);
						printf("\t\tDisk maximum read asynch IO callbacks : %u\n", repo_config->disk_max_read_aiocbs);
						printf("\t\tDisk maximum write asynch IO callbacks: %u\n", repo_config->disk_max_write_aiocbs);
						printf("\t\tDmon topic index                      : %u\n", repo_config->dmon_topic_idx);
						printf("\t\tOriginating transport ID              : %s\n", repo_config->otid);
						printf("\t\tRepository disk size limit            : %lu\n", repo_config->repo_disk_sz_limit);
						printf("\t\tDisk write delay                      : %u\n", repo_config->repo_disk_write_delay);
						printf("\t\tRepository size limit                 : %u\n", repo_config->repo_sz_limit);
						printf("\t\tRepository size threshold             : %u\n", repo_config->repo_sz_threshold);
						printf("\t\tSource flight size bytes              : %lu\n", repo_config->src_flightsz_bytes);
						if (repo_config->n_rcv_configs > 0) {
							int rcvr_idx;
							for (rcvr_idx = 0; rcvr_idx < repo_config->n_rcv_configs; ++rcvr_idx) {
								Lbmmon__UMPMonMsg__Configs__TopicConfig__RepoConfig__RcvConfig *rcvr_config = repo_config->rcv_configs[rcvr_idx];
								addr.s_addr = rcvr_config->ip_addr;
								printf("\tReceiver configuration %d\n", rcvr_idx);
								printf("\t\tReceiver session ID     : %lu\n", rcvr_config->rcv_session_id);
								printf("\t\tReceiver registration ID: %u\n", rcvr_config->rcv_regid);
								printf("\t\tSource registration ID  : %u\n", rcvr_config->src_regid);
								printf("\t\tDmon topic index        : %u\n", rcvr_config->dmon_topic_idx);
								printf("\t\tIP address              : %s\n", inet_ntoa(addr));
								printf("\t\tPort                    : %u\n", ntohs(rcvr_config->port));
								printf("\t\tDomain ID               : %u\n", rcvr_config->domain_id);
								printf("\t\tTransport index         : %u\n", rcvr_config->transport_idx);
								printf("\t\tTopic index             : %u\n", rcvr_config->topic_idx);
							}
						}
					}
				}
			}
		}
	}
	if (store_lbmmon_msg->events != NULL) {
		Lbmmon__UMPMonMsg__Events *events = store_lbmmon_msg->events;

		print_attributes("Store events received", AttributeBlock, 1);
		if (store_lbmmon_msg->events->n_events > 0) {
			int event_idx;
			for (event_idx = 0; event_idx < events->n_events; ++event_idx) {
				Lbmmon__UMPMonMsg__Events__Event *event = events->events[event_idx];
				printf("\tEvent number: %d\n", event_idx);
				printf("\t\tEvent type             : %u\n", event->event_type);
				printf("\t\tStore index            : %u\n", event->store_idx);
				printf("\t\tTimestamp seconds      : %lu\n", event->timestamp_sec);
				printf("\t\tTimestamp microseconds : %lu\n", event->timestamp_usec);
				printf("\t\tLead sequence number   : %u\n", event->lead_sqn);
				printf("\t\tLow sequence number    : %u\n", event->low_sqn);
				printf("\t\tHigh sequence number   : %d\n", event->high_sqn);
				printf("\t\tSource registration ID : %u\n", event->src_regid);
				printf("\t\tReceive registration ID: %u\n", event->rcv_regid);
				printf("\t\tDeletion reason code   : %u\n", event->deletion_reason_code);
				printf("\t\tTopic name             : %s\n", event->topic_name);
				printf("\t\tDmon topic index       : %u\n", event->dmon_topic_idx);
			}

		}
	}
	fflush(stdout);
}


void tnwg_show_lbmmon_stats_othergwstat_otherportalblock(Lbmmon__DROMonMsg__Stats__OtherGateway__OtherPortal ** otherportals_array, int n_other_portals)
{
	int p = 0;
	for(; p < n_other_portals; p++){
		INCT();
		printf("%s--- Other Portals[%d]---\n", PRTT(), p);
		printf("%scost                : %d\n", PRTT(), otherportals_array[p]->cost);
		printf("%sportal_type_case    : %d\n", PRTT(), otherportals_array[p]->portal_type_case);
		if(otherportals_array[p]->portal_type_case == LBMMON__DROMON_MSG__STATS__OTHER_GATEWAY__OTHER_PORTAL__PORTAL_TYPE_ADJACENT_DOMAIN_ID) {
			printf("%sadjacent_domain_id  : %lu\n", PRTT(), otherportals_array[p]->adjacent_domain_id);
		} else {
			printf("%sadjacent_gateway_id : %lu\n", PRTT(), otherportals_array[p]->adjacent_gateway_id);
		}
		DECT();
	}
}

void tnwg_show_lbmmon_stats_othergwstat(Lbmmon__DROMonMsg__Stats__OtherGateway * othergwstat)
{

	INCT();
	printf("%sOther Gateway Name : (%s)---\n", PRTT(), othergwstat->gateway_name);
	printf("%sgateway_id         : %lu\n", PRTT(), othergwstat->gateway_id);
	printf("%sversion            :  %d\n", PRTT(), othergwstat->version);
	printf("%stopology_signature : 0x%08x\n", PRTT(), othergwstat->topology_signature);
	printf("%slast_activity_sec  :  %ld\n", PRTT(), othergwstat->last_activity_sec);
	printf("%slast_activity_usec :  %ld\n", PRTT(), othergwstat->last_activity_usec);
	printf("%sn_other_portals    :  %d\n", PRTT(), (int) othergwstat->n_other_portals);
	
	DECT();
	if (othergwstat->n_other_portals > 0) {
		INCT();
		printf("%s--- Other Gateway Stats - Portals ---\n", PRTT());
		tnwg_show_lbmmon_stats_othergwstat_otherportalblock(othergwstat->other_portals, othergwstat->n_other_portals);
		DECT();
	}
}

void tnwg_show_lbmmon_stats_othergwstatsblock(Lbmmon__DROMonMsg__Stats__OtherGateway ** othergwstats_array, int n_other_gateways)
{
	int g = 0;
	INCT();
	printf("%s--- Other Gateway Stats[] ---\n", PRTT());
	for(; g < n_other_gateways; g++){
		INCT();
		printf("%s--- Other Gateway[%d]---\n", PRTT(), g);
		DECT();
		tnwg_show_lbmmon_stats_othergwstat(othergwstats_array[g]);
	}
	DECT();
}

void tnwg_show_lbmmon_stats_portalstats_peerstats_receive( Lbmmon__DROMonMsg__Stats__Portal__Peer__Receive *peer_receive)
{
	INCT();
	printf("%sData messages received                                                  : %ld\n",PRTT(), peer_receive->data_msgs_rcvd);
	printf("%sData bytes received                                                     : %ld\n",PRTT(), peer_receive->data_msg_bytes_rcvd);
	printf("%sTransport topic fragment data messages received                         : %ld\n",PRTT(), peer_receive->transport_topic_fragment_data_msgs_rcvd);
	printf("%sTransport topic fragment data messages received bytes                   : %ld\n",PRTT(), peer_receive->transport_topic_fragment_data_msg_bytes_rcvd);
	printf("%sTransport topic fragment data messages received with unknown source     : %ld\n",PRTT(), peer_receive->transport_topic_fragment_data_msgs_rcvd_unknown_source);
	printf("%sTransport topic fragment data bytes received with unknown source        : %ld\n",PRTT(), peer_receive->transport_topic_fragment_data_msg_bytes_rcvd_unknown_source);
	printf("%sTransport topic request fragment data messages received                 : %ld\n",PRTT(), peer_receive->transport_topic_req_fragment_data_msgs_rcvd);
	printf("%sTransport topic request fragment data bytes received                    : %ld\n",PRTT(), peer_receive->transport_topic_req_fragment_data_msg_bytes_rcvd);
	printf("%sTransport topic req frag. data messages received with unknown source    : %ld\n",PRTT(), peer_receive->transport_topic_req_fragment_data_msgs_rcvd_unknown_source);
	printf("%sTransport topic req frag. data bytes received with unknown source       : %ld\n",PRTT(), peer_receive->transport_topic_req_fragment_data_msg_bytes_rcvd_unknown_source);
	printf("%sTransport topic control messages received                               : %ld\n",PRTT(), peer_receive->transport_topic_control_msgs_rcvd);
	printf("%sTransport topic control bytes received                                  : %ld\n",PRTT(), peer_receive->transport_topic_control_msg_bytes_rcvd);
	printf("%sTransport topic control messages received with unknown source           : %ld\n",PRTT(), peer_receive->transport_topic_control_msgs_rcvd_unknown_source);
	printf("%sTransport topic control bytes received with unknown source              : %ld\n",PRTT(), peer_receive->transport_topic_control_msg_bytes_rcvd_unknown_source);
	printf("%sImmediate topic fragment data messages received                         : %ld\n",PRTT(), peer_receive->immediate_topic_fragment_data_msgs_rcvd);
	printf("%sImmediate topic fragment data bytes received                            : %ld\n",PRTT(), peer_receive->immediate_topic_fragment_data_msg_bytes_rcvd);
	printf("%sImmediate topic request fragment data messages received                 : %ld\n",PRTT(), peer_receive->immediate_topic_req_fragment_data_msgs_rcvd);
	printf("%sImmediate topic request fragment data bytes received                    : %ld\n",PRTT(), peer_receive->immediate_topic_req_fragment_data_msg_bytes_rcvd);
	printf("%sImmediate topicless fragment data messages received                     : %ld\n",PRTT(), peer_receive->immediate_topicless_fragment_data_msgs_rcvd);
	printf("%sImmediate topicless fragment data bytes received                        : %ld\n",PRTT(), peer_receive->immediate_topicless_fragment_data_msg_bytes_rcvd);
	printf("%sImmediate topicless request fragment data messages received             : %ld\n",PRTT(), peer_receive->immediate_topicless_req_fragment_data_msgs_rcvd);
	printf("%sImmediate topicless request fragment data bytes received                : %ld\n",PRTT(), peer_receive->immediate_topicless_req_fragment_data_msg_bytes_rcvd);
	printf("%sUnicast data messages received                                          : %ld\n",PRTT(), peer_receive->unicast_data_msgs_rcvd);
	printf("%sUnicast data bytes received                                             : %ld\n",PRTT(), peer_receive->unicast_data_msg_bytes_rcvd);
	printf("%sUnicast data messages received with no forwarding information           : %ld\n",PRTT(), peer_receive->unicast_data_msgs_rcvd_no_fwd);
	printf("%sUnicast data message bytes received with no forwarding information      : %ld\n",PRTT(), peer_receive->unicast_data_msg_bytes_rcvd_no_fwd);
	printf("%sUnicast data messages received with unknown forwarding information      : %ld\n",PRTT(), peer_receive->unicast_data_msgs_rcvd_unknown_fwd);
	printf("%sUnicast data message bytes received with unknown forwarding information : %ld\n",PRTT(), peer_receive->unicast_data_msg_bytes_rcvd_unknown_fwd);
	printf("%sUnicast data messages received with no stream information               : %ld\n",PRTT(), peer_receive->unicast_data_msgs_rcvd_no_stream);
	printf("%sUnicast data message bytes received with no stream information          : %ld\n",PRTT(), peer_receive->unicast_data_msg_bytes_rcvd_no_stream);
	printf("%sUnicast data messages dropped no route                                  : %ld\n",PRTT(), peer_receive->unicast_data_msgs_dropped_no_route);
	printf("%sUnicast data message bytes dropped no route                             : %ld\n",PRTT(), peer_receive->unicast_data_msg_bytes_dropped_no_route);
	printf("%sControl messages received                                               : %ld\n",PRTT(), peer_receive->cntl_msgs_rcvd);
	printf("%sControl message bytes received                                          : %ld\n",PRTT(), peer_receive->cntl_msg_bytes_rcvd);
	printf("%sUnicast control messages received                                       : %ld\n",PRTT(), peer_receive->unicast_cntl_msgs_rcvd);
	printf("%sUnicast control message  bytes received                                 : %ld\n",PRTT(), peer_receive->unicast_cntl_msg_bytes_rcvd);
	printf("%sUnicast Control retransmission requests received                        : %ld\n",PRTT(), peer_receive->unicast_cntl_rxreq_msgs_rcvd);
	printf("%sUnicast Control retransmission bytes received                           : %ld\n",PRTT(), peer_receive->unicast_cntl_rxreq_msg_bytes_rcvd);
	printf("%sUnicast control messages received but unhandled                         : %ld\n",PRTT(), peer_receive->unicast_cntl_msgs_rcvd_unhandled);
	printf("%sUnicast control message bytes received but unhandled                    : %ld\n",PRTT(), peer_receive->unicast_cntl_msg_bytes_rcvd_unhandled);
	printf("%sUnicast control messages received with no stream information            : %ld\n",PRTT(), peer_receive->unicast_cntl_msgs_rcvd_no_stream);
	printf("%sUnicast control message bytes received with no stream information       : %ld\n",PRTT(), peer_receive->unicast_cntl_msg_bytes_rcvd_no_stream);
	printf("%sUnicast control messages dropped no route                               : %ld\n",PRTT(), peer_receive->unicast_cntl_msgs_dropped_no_route);
	printf("%sUnicast control message bytes dropped no route                          : %ld\n",PRTT(), peer_receive->unicast_cntl_msg_bytes_dropped_no_route);
	printf("%sGateway control messages received                                       : %ld\n",PRTT(), peer_receive->gateway_cntl_msgs_rcvd);
	printf("%sGateway control message bytes received                                  : %ld\n",PRTT(), peer_receive->gateway_cntl_msg_bytes_rcvd);
	DECT();
		
}
void tnwg_show_lbmmon_stats_portalstats_peerstats_send( Lbmmon__DROMonMsg__Stats__Portal__Peer__Send *peer_send)
{
	INCT();
	printf("%sData fragments forwarded to this portal                                   : %ld\n", PRTT(), peer_send->data_fragments_forwarded);
	printf("%sData fragment bytes forwarded to this portal                              : %ld\n", PRTT(), peer_send->data_fragment_bytes_forwarded);
	printf("%sData fragments sent                                                       : %ld\n", PRTT(), peer_send->data_fragments_sent);
	printf("%sData fragment bytes sent                                                  : %ld\n", PRTT(), peer_send->data_fragment_bytes_sent);
	printf("%sDuplicate data fragments dropped                                          : %ld\n", PRTT(), peer_send->data_fragments_dropped_dup);
	printf("%sDuplicate data fragment bytes dropped                                     : %ld\n", PRTT(), peer_send->data_fragment_bytes_dropped_dup);
	printf("%sData fragments dropped due to EWOULDBLOCK                                 : %ld\n",PRTT(), peer_send->data_fragments_dropped_would_block);
	printf("%sData fragment bytes dropped due to EWOULDBLOCK                            : %ld\n",PRTT(), peer_send->data_fragment_bytes_dropped_would_block);
	printf("%sData fragments dropped due to portal not being operational                : %ld\n", PRTT(), peer_send->data_fragments_dropped_not_operational);
	printf("%sData fragment bytes dropped due to portal not being operational           : %ld\n", PRTT(), peer_send->data_fragment_bytes_dropped_not_operational);
	printf("%sData fragments dropped due to queueing failure                            : %ld\n",PRTT(), peer_send->data_fragments_dropped_queue_failure);
	printf("%sData fragment bytes dropped due to queueing failure                       : %ld\n", PRTT(), peer_send->data_fragment_bytes_dropped_queue_failure);
	printf("%sUnicast messages forwarded to this portal                                 : %ld\n", PRTT(), peer_send->unicast_msgs_forwarded);
	printf("%sUnicast message bytes forwarded to this portal                            : %ld\n", PRTT(), peer_send->unicast_msg_bytes_forwarded);
	printf("%sUnicast messages sent                                                     : %ld\n", PRTT(), peer_send->unicast_msgs_sent);
	printf("%sUnicast message bytes sent                                                : %ld\n", PRTT(), peer_send->unicast_msg_bytes_sent);
	printf("%sUnicast messages dropped due to EWOULDBLOCK                               : %ld\n", PRTT(), peer_send->unicast_msgs_dropped_would_block);
	printf("%sUnicast message bytes dropped due to EWOULDBLOCK                          : %ld\n", PRTT(), peer_send->unicast_msg_bytes_dropped_would_block);
	printf("%sUnicast messages dropped due to portal not being operational              : %ld\n", PRTT(), peer_send->unicast_msgs_dropped_not_operational);
	printf("%sUnicast message bytes dropped due to portal not being operational         : %ld\n", PRTT(), peer_send->unicast_msg_bytes_dropped_not_operational);
	printf("%sUnicast messages dropped due to queueing failure                          : %ld\n", PRTT(), peer_send->unicast_msgs_dropped_queue_failure);
	printf("%sUnicast message bytes dropped due to queueing failure                     : %ld\n", PRTT(), peer_send->unicast_msg_bytes_dropped_queue_failure);
	printf("%sGateway control messages                                                  : %ld\n", PRTT(), peer_send->gateway_cntl_msgs);
	printf("%sGateway control message bytes                                             : %ld\n", PRTT(), peer_send->gateway_cntl_msg_bytes);
	printf("%sGateway control messages sent                                             : %ld\n", PRTT(), peer_send->gateway_cntl_msgs_sent);
	printf("%sGateway control message bytes sent                                        : %ld\n", PRTT(), peer_send->gateway_cntl_msg_bytes_sent);
	printf("%sGateway control messages dropped due to EWOULDBLOCK                       : %ld\n", PRTT(), peer_send->gateway_cntl_msgs_dropped_would_block);
	printf("%sGateway control message bytes dropped due to EWOULDBLOCK                  : %ld\n", PRTT(), peer_send->gateway_cntl_msg_bytes_dropped_would_block);
	printf("%sGateway control messages dropped due to portal not being operational      : %ld\n", PRTT(), peer_send->gateway_cntl_msgs_dropped_not_operational);
	printf("%sGateway control message bytes dropped due to portal not being operational : %ld\n", PRTT(), peer_send->gateway_cntl_msg_bytes_dropped_not_operational);
	printf("%sGateway control messages dropped due to queueing failure                  : %ld\n", PRTT(), peer_send->gateway_cntl_msgs_dropped_queue_failure);
	printf("%sGateway control message bytes dropped due to queueing failure             : %ld\n", PRTT(), peer_send->gateway_cntl_msg_bytes_dropped_queue_failure);
	printf("%sNumber of message batches                                                 : %ld\n", PRTT(), peer_send->batches);
	printf("%sMinimum number of messages per batch                                      : %ld\n", PRTT(), peer_send->batch_msgs_min);
	printf("%sMean number of messages per batch                                         : %ld\n", PRTT(), peer_send->batch_msgs_mean); 
	printf("%sMaximum number of messages per batch                                      : %ld\n", PRTT(), peer_send->batch_msgs_max);
	printf("%sMinimum number of bytes per batch                                         : %ld\n", PRTT(), peer_send->batch_bytes_min);
	printf("%sMean number of bytes per batch                                            : %ld\n", PRTT(), peer_send->batch_bytes_mean);
	printf("%sMaximum number of bytes per batch                                         : %ld\n", PRTT(), peer_send->batch_bytes_max);
	printf("%sCurrent data bytes enqueued internally                                    : %ld\n", PRTT(), peer_send->data_bytes_enqueued);
	printf("%sMaximum data bytes enqueued internally                                    : %ld\n", PRTT(), peer_send->data_bytes_enqueued_max);
	printf("%sConfigured maximum data bytes allowed in queued                           : %ld\n", PRTT(), peer_send->data_bytes_enqueued_limit);
	printf("%sCurrent UIM Message bytes queued internally                               : %ld\n", PRTT(), peer_send->unicast_msg_bytes_enqueued);
	printf("%sMaximum UIM Message bytes queued                                          : %ld\n", PRTT(), peer_send->unicast_msg_bytes_enqueued_max);
	printf("%sUIM Message bytes Queue Limit                                             : %ld\n", PRTT(), peer_send->unicast_msg_bytes_enqueued_limit);
	printf("%sTotal RTT samples                                                         : %ld\n", PRTT(), peer_send->rtt_samples);
	printf("%sMinimum RTT to companion(microseconds)                                    : %ld\n", PRTT(), peer_send->rtt_min);
	printf("%sMean RTT to companion(microseconds)                                       : %ld\n",PRTT(), peer_send->rtt_mean);
	printf("%sMaximum RTT to companion(microseconds)                                    : %ld\n", PRTT(), peer_send->rtt_max);
	printf("%sLast Keepalive responded to                                               : %ld\n", PRTT(), peer_send->last_ka_time);
	DECT();
	
}

void tnwg_show_lbmmon_stats_portalstats_peerstats(Lbmmon__DROMonMsg__Stats__Portal__Peer *peer)
{
	INCT();
	printf("%sAdjacent Gateway ID     : %lu\n", PRTT(), peer->adjacent_gateway_id );
	printf("%sInterest Topics         : %d\n", PRTT(), peer->interest_topics );
	printf("%sInterest PCRE Patterns  : %d\n", PRTT(), peer->interest_pcre_patterns );
	printf("%sInterest REGEX Patterns : %d\n", PRTT(), peer->interest_regex_patterns );
	if (peer->receive != NULL) {
		INCT();
		printf("%s--- Peer Receive Stats ---\n", PRTT());
		DECT();
		tnwg_show_lbmmon_stats_portalstats_peerstats_receive(peer->receive);
	}
	if (peer->send != NULL) {
		INCT();
		printf("%s--- Peer Send Stats ---\n", PRTT());
		DECT();
		tnwg_show_lbmmon_stats_portalstats_peerstats_send(peer->send);
	}
}

void tnwg_show_lbmmon_stats_portalstats_endptstats_receive( Lbmmon__DROMonMsg__Stats__Portal__Endpoint__Receive *ept_receive)
{
	INCT();
	printf("%sTransport topic message fragments received                           : %ld\n",PRTT(), ept_receive->transport_topic_fragments_rcvd);
	printf("%sTransport topic message fragment bytes received                      : %ld\n",PRTT(), ept_receive->transport_topic_fragment_bytes_rcvd);
	printf("%sTransport topic message request fragments received                   : %ld\n",PRTT(), ept_receive->transport_topic_req_fragments_rcvd);
	printf("%sTransport topic message request fragment bytes received              : %ld\n",PRTT(), ept_receive->transport_topic_req_fragment_bytes_rcvd);
	printf("%sTransport topic control message received                             : %ld\n",PRTT(), ept_receive->transport_topic_control_rcvd);
	printf("%sTransport topic control message bytes received                       : %ld\n",PRTT(), ept_receive->transport_topic_control_bytes_rcvd);
	printf("%sImmediate topic message fragments received                           : %ld\n",PRTT(), ept_receive->immediate_topic_fragments_rcvd);
	printf("%sImmediate topic message fragment bytes received                      : %ld\n",PRTT(), ept_receive->immediate_topic_fragment_bytes_rcvd);
	printf("%sImmediate topic message request fragments received                   : %ld\n",PRTT(), ept_receive->immediate_topic_req_fragments_rcvd);
	printf("%sImmediate topic message request fragment bytes received              : %ld\n",PRTT(), ept_receive->immediate_topic_req_fragment_bytes_rcvd);
	printf("%sImmediate topicless message fragments received                       : %ld\n",PRTT(), ept_receive->immediate_topicless_fragments_rcvd);
	printf("%sImmediate topicless message fragment bytes received                  : %ld\n",PRTT(), ept_receive->immediate_topicless_fragment_bytes_rcvd);
	printf("%sImmediate topicless message request fragments received               : %ld\n",PRTT(), ept_receive->immediate_topicless_req_fragments_rcvd);
	printf("%sImmediate topicless message request fragment bytes received          : %ld\n",PRTT(), ept_receive->immediate_topicless_req_fragment_bytes_rcvd);
	printf("%sUnicast data messages received                                       : %ld\n",PRTT(), ept_receive->unicast_data_msgs_rcvd);
	printf("%sUnicast data message bytes received                                  : %ld\n",PRTT(), ept_receive->unicast_data_msg_bytes_rcvd);
	printf("%sUnicast data messages received with no stream identification         : %ld\n",PRTT(), ept_receive->unicast_data_msgs_rcvd_no_stream);
	printf("%sUnicast data message bytes received with no stream identification    : %ld\n",PRTT(), ept_receive->unicast_data_msg_bytes_rcvd_no_stream);
	printf("%sUnicast data messages dropped as duplicates                          : %ld\n",PRTT(), ept_receive->unicast_data_msgs_dropped_dup);
	printf("%sUnicast data message bytes dropped as duplicates                     : %ld\n",PRTT(), ept_receive->unicast_data_msg_bytes_dropped_dup);
	printf("%sUnicast data messages dropped no route                               : %ld\n",PRTT(), ept_receive->unicast_data_msgs_dropped_no_route);
	printf("%sUnicast data message bytes dropped no route                          : %ld\n",PRTT(), ept_receive->unicast_data_msg_bytes_dropped_no_route);
	printf("%sUnicast control messages received                                    : %ld\n",PRTT(), ept_receive->unicast_cntl_msgs_rcvd);
	printf("%sUnicast control message bytes received                               : %ld\n",PRTT(), ept_receive->unicast_cntl_msg_bytes_rcvd);
	printf("%sUnicast control messages received with no stream identification      : %ld\n",PRTT(), ept_receive->unicast_cntl_msgs_rcvd_no_stream);
	printf("%sUnicast control message bytes received with no stream identification : %ld\n",PRTT(), ept_receive->unicast_cntl_msg_bytes_rcvd_no_stream);
	printf("%sUnicast control messages dropped as duplicates                       : %ld\n",PRTT(), ept_receive->unicast_cntl_msgs_dropped_dup);
	printf("%sUnicast control message bytes dropped as duplicates                  : %ld\n",PRTT(), ept_receive->unicast_cntl_msg_bytes_dropped_dup);
	printf("%sUnicast control messages dropped no route                            : %ld\n",PRTT(), ept_receive->unicast_cntl_msgs_dropped_no_route);
	printf("%sUnicast control message bytes dropped no route                       : %ld\n",PRTT(), ept_receive->unicast_cntl_msg_bytes_dropped_no_route);
	DECT();
}


void tnwg_show_lbmmon_stats_portalstats_endptstats_send( Lbmmon__DROMonMsg__Stats__Portal__Endpoint__Send *ept_send)
{
	INCT();
	printf("%sTransport topic fragments forwarded to this portal                    : %ld\n", PRTT(),ept_send->transport_topic_fragments_forwarded);
	printf("%sTransport topic fragment bytes forwarded to this portal               : %ld\n", PRTT(),ept_send->transport_topic_fragment_bytes_forwarded);
	printf("%sTransport topic fragments sent                                        : %ld\n", PRTT(),ept_send->transport_topic_fragments_sent);
	printf("%sTransport topic fragment bytes sent                                   : %ld\n", PRTT(),ept_send->transport_topic_fragment_bytes_sent);
	printf("%sTransport topic request fragments sent                                : %ld\n", PRTT(),ept_send->transport_topic_req_fragments_sent);
	printf("%sTransport topic request fragment bytes sent                           : %ld\n", PRTT(),ept_send->transport_topic_req_fragment_bytes_sent);
	printf("%sDuplicate transport topic fragments dropped                           : %ld\n", PRTT(),ept_send->transport_topic_fragments_dropped_dup);
	printf("%sDuplicate transport topic fragment bytes dropped                      : %ld\n", PRTT(),ept_send->transport_topic_fragment_bytes_dropped_dup);
	printf("%sTransport topic fragments dropped due to EWOULDBLOCK                  : %ld\n", PRTT(),ept_send->transport_topic_fragments_dropped_would_block);
	printf("%sTransport topic fragment bytes dropped due to EWOULDBLOCK             : %ld\n", PRTT(),ept_send->transport_topic_fragment_bytes_dropped_would_block);
	printf("%sTransport topic fragments dropped due to error                        : %ld\n", PRTT(),ept_send->transport_topic_fragments_dropped_error); 
	printf("%sTransport topic fragment bytes dropped due to error                   : %ld\n", PRTT(),ept_send->transport_topic_fragment_bytes_dropped_error);
	printf("%sTransport topic fragments dropped due to fragment size error          : %ld\n", PRTT(),ept_send->transport_topic_fragments_dropped_size_error);
	printf("%sTransport topic fragment bytes dropped due to fragment size error     : %ld\n", PRTT(),ept_send->transport_topic_fragment_bytes_dropped_size_error);
	printf("%sImmediate topic fragments forwarded                                   : %ld\n", PRTT(),ept_send->immediate_topic_fragments_forwarded);
	printf("%sImmediate topic fragment bytes forwarded                              : %ld\n", PRTT(),ept_send->immediate_topic_fragment_bytes_forwarded);
	printf("%sImmediate topic fragments sent                                        : %ld\n", PRTT(),ept_send->immediate_topic_fragments_sent);
	printf("%sImmediate topic fragment bytes sent                                   : %ld\n", PRTT(),ept_send->immediate_topic_fragment_bytes_sent);
	printf("%sImmediate topic request fragments sent                                : %ld\n", PRTT(),ept_send->immediate_topic_req_fragments_sent);
	printf("%sImmediate topic request fragment bytes sent                           : %ld\n", PRTT(),ept_send->immediate_topic_req_fragment_bytes_sent);
	printf("%sImmediate topic fragments dropped due to EWOULDBLOCK                  : %ld\n", PRTT(),ept_send->immediate_topic_fragments_dropped_would_block);
	printf("%sImmediate topic fragment bytes dropped due to EWOULDBLOCK             : %ld\n", PRTT(),ept_send->immediate_topic_fragment_bytes_dropped_would_block);
	printf("%sImmediate topic fragments dropped due to error                        : %ld\n", PRTT(),ept_send->immediate_topic_fragments_dropped_error);
	printf("%sImmediate topic fragment bytes dropped due to error                   : %ld\n", PRTT(),ept_send->immediate_topic_fragment_bytes_dropped_error);
	printf("%sImmediate topic fragments dropped due to fragment size error          : %ld\n", PRTT(),ept_send->immediate_topic_fragments_dropped_size_error);
	printf("%sImmediate topic fragment bytes dropped due to fragment size error     : %ld\n", PRTT(),ept_send->immediate_topic_fragment_bytes_dropped_size_error);
	printf("%sImmediate topicless fragments forwarded                               : %ld\n", PRTT(),ept_send->immediate_topicless_fragments_forwarded);
	printf("%sImmediate topicless fragment bytes forwarded                          : %ld\n", PRTT(),ept_send->immediate_topicless_fragment_bytes_forwarded);
	printf("%sImmediate topicless fragments sent                                    : %ld\n", PRTT(),ept_send->immediate_topicless_fragments_sent);
	printf("%sImmediate topicless fragment bytes sent                               : %ld\n", PRTT(),ept_send->immediate_topicless_fragment_bytes_sent);
	printf("%sImmediate topicless request fragments sent                            : %ld\n", PRTT(),ept_send->immediate_topicless_req_fragments_sent);
	printf("%sImmediate topicless request fragment bytes sent                       : %ld\n", PRTT(),ept_send->immediate_topicless_req_fragment_bytes_sent);
	printf("%sImmediate topicless fragments dropped due to EWOULDBLOCK              : %ld\n", PRTT(),ept_send->immediate_topicless_fragments_dropped_would_block);
	printf("%sImmediate topicless fragment bytes dropped due to EWOULDBLOCK         : %ld\n", PRTT(),ept_send->immediate_topicless_fragment_bytes_dropped_would_block);
	printf("%sImmediate topicless fragments dropped due to error                    : %ld\n", PRTT(),ept_send->immediate_topicless_fragments_dropped_error);
	printf("%sImmediate topicless fragment bytes dropped due to error               : %ld\n", PRTT(),ept_send->immediate_topicless_fragment_bytes_dropped_error);
	printf("%sImmediate topicless fragments dropped due to fragment size error      : %ld\n", PRTT(),ept_send->immediate_topicless_fragments_dropped_size_error);
	printf("%sImmediate topicless fragment bytes dropped due to fragment size error : %ld\n", PRTT(),ept_send->immediate_topicless_fragment_bytes_dropped_size_error);
	printf("%sUnicast messages forwarded                                            : %ld\n", PRTT(),ept_send->unicast_msgs_forwarded);
	printf("%sUnicast message bytes forwarded                                       : %ld\n", PRTT(),ept_send->unicast_msg_bytes_forwarded);
	printf("%sUnicast messages sent                                                 : %ld\n", PRTT(),ept_send->unicast_msgs_sent);
	printf("%sUnicast message bytes sent                                            : %ld\n", PRTT(),ept_send->unicast_msg_bytes_sent);
	printf("%sUnicast messages dropped due to error                                 : %ld\n", PRTT(),ept_send->unicast_msgs_dropped_error);
	printf("%sUnicast message bytes dropped due to error                            : %ld\n", PRTT(),ept_send->unicast_msg_bytes_dropped_error);
	printf("%sCurrent data bytes enqueued internally                                : %ld\n", PRTT(),ept_send->data_bytes_enqueued);
	printf("%sMaximum data bytes enqueued internally                                : %ld\n", PRTT(),ept_send->data_bytes_enqueued_max);
	printf("%sConfigured maximum data bytes allowed in queued                       : %ld\n", PRTT(),ept_send->data_bytes_enqueued_limit);
	DECT();	
	
}

void tnwg_show_lbmmon_stats_portalstats_endptstats(Lbmmon__DROMonMsg__Stats__Portal__Endpoint * endpoint_stats)
{
	INCT();
	printf("%sDomain ID                      : %d\n", PRTT(), endpoint_stats->domain_id);
	printf("%sLocal Interest Topics          : %d\n", PRTT(), endpoint_stats->local_interest_topics);
	printf("%sLocal Interest PCRE Patterns   : %d\n", PRTT(), endpoint_stats->local_interest_pcre_patterns);
	printf("%sLocal Interest REGEX Patterns  : %d\n", PRTT(), endpoint_stats->local_interest_regex_patterns);
	printf("%sRemote Interest Topics         : %d\n", PRTT(), endpoint_stats->remote_interest_topics);
	printf("%sRemote Interest PCRE Patterns  : %d\n", PRTT(), endpoint_stats->remote_interest_pcre_patterns);
	printf("%sRemote Interest REGEX Patterns : %d\n", PRTT(), endpoint_stats->remote_interest_regex_patterns);

	if (endpoint_stats->receive != NULL) {
		INCT();
		printf("%s--- Endpoint Receive Stats ---\n", PRTT());
		DECT();
		tnwg_show_lbmmon_stats_portalstats_endptstats_receive(endpoint_stats->receive);
	}
	if (endpoint_stats->send != NULL) {
		INCT();
		printf("%s--- Endpoint Send Stats ---\n", PRTT());
		DECT();
		tnwg_show_lbmmon_stats_portalstats_endptstats_send(endpoint_stats->send);
	}
}

void tnwg_show_lbmmon_stats_portalstats(Lbmmon__DROMonMsg__Stats__Portal * portals)
{	
	INCT();	
	printf("%sPortal Name                    : %s \n", PRTT(), portals->portal_name);
	printf("%sportal_index                   : %d\n", PRTT(), portals->portal_index);
	printf("%sportal_type_case               : %d\n", PRTT(), portals->portal_type_case);
	printf("%scost                           : %d\n", PRTT(), portals->cost);
	printf("%sproxy_receivers                : %d\n", PRTT(), portals->proxy_receivers);
	printf("%sreceiver_topics                : %d\n", PRTT(), portals->receiver_topics);
	printf("%sreceiver_pcre_patterns         : %d\n", PRTT(), portals->receiver_pcre_patterns);
	printf("%sreceiver_regex_patterns        : %d\n", PRTT(), portals->receiver_regex_patterns);
	printf("%sproxy_sources                  : %d\n", PRTT(), portals->proxy_sources);
	printf("%srecalc_duration_sec            : %ld\n", PRTT(), portals->recalc_duration_sec);
	printf("%srecalc_duration_usec           : %ld\n", PRTT(), portals->recalc_duration_usec);
	printf("%sproxy_rec_recalc_duration_sec  : %ld\n", PRTT(), portals->proxy_rec_recalc_duration_sec);
	printf("%sproxy_rec_recalc_duration_usec : %ld\n", PRTT(), portals->proxy_rec_recalc_duration_usec);
	DECT();
	if (portals->portal_type_case == LBMMON__DROMON_MSG__STATS__PORTAL__PORTAL_TYPE_ENDPOINT) {
		if (portals->endpoint != NULL) {
			INCT();
			printf("%s---Portal Endpoint Stats ---\n", PRTT());
			tnwg_show_lbmmon_stats_portalstats_endptstats(portals->endpoint);
		} else {
			INCT();
			printf("%s NO Portal Endpoint Stats ---\n", PRTT());
		}
		DECT();
	}

	if (portals->portal_type_case == LBMMON__DROMON_MSG__STATS__PORTAL__PORTAL_TYPE_PEER) {
		if (portals->peer != NULL) {
			INCT();
			printf("%s--- Portal Peer Stats ---\n", PRTT());
			tnwg_show_lbmmon_stats_portalstats_peerstats(portals->peer);
		} else {
			INCT();
			printf("%s NO Portal Peer Stats ---\n", PRTT());
		}
		DECT();
	}
}

void tnwg_show_lbmmon_stats_portalstatsblocks(Lbmmon__DROMonMsg__Stats__Portal ** portal_stats_array, int n_portals)
{
	int p = 0;
	INCT();
	printf("%s--- Portal Stats[] ---\n", PRTT());
	for(; p< n_portals; p++) {
		INCT();
		printf("%s--- Portal[%d] ---\n", PRTT(), p);
		tnwg_show_lbmmon_stats_portalstats(portal_stats_array[p]);
		DECT();
	}
	DECT();
}

void tnwg_show_lbmmon_stats_localstats_mallocinfo(Lbmmon__DROMonMsg__Stats__Local__MallocInfo * malloc_info)
{
	INCT();

	printf("%sarena    : %d\n", PRTT(), malloc_info->arena);
	printf("%sordblks  : %d\n", PRTT(), malloc_info->ordblks);
	printf("%shblks    : %d\n", PRTT(), malloc_info->hblks);
	printf("%shblkhd   : %d\n", PRTT(), malloc_info->hblkhd);
	printf("%suordblks : %d\n", PRTT(), malloc_info->uordblks);
	printf("%sfordblks : %d\n", PRTT(), malloc_info->fordblks);
	DECT();
}

void tnwg_show_lbmmon_stats_localstats(Lbmmon__DROMonMsg__Stats__Local *localstats)
{
	INCT();
	printf("%sgateway_name         : %s\n", PRTT(), localstats->gateway_name);
	printf("%sgateway_id           : %lu\n", PRTT(), localstats->gateway_id);
	printf("%sversion              : %d\n", PRTT(), localstats->version);
	printf("%stopology_signature   : 0x%08x\n", PRTT(), localstats->topology_signature);
	printf("%srecalc_duration_sec  : %ld\n", PRTT(), localstats->recalc_duration_sec);
	printf("%srecalc_duration_usec : %ld\n", PRTT(), localstats->recalc_duration_usec);
	printf("%sgraph_version        : %d\n", PRTT(), localstats->graph_version);
	printf("%sgateway_count        : %d\n", PRTT(), localstats->gateway_count);
	printf("%strd_count            : %d\n", PRTT(), localstats->trd_count);
	DECT();
	if (localstats->malloc_info != NULL) {
		printf("%s----Malloc info----\n", PRTT());
		tnwg_show_lbmmon_stats_localstats_mallocinfo(localstats->malloc_info);
	}


}

void tnwg_show_lbmmon_stats(Lbmmon__DROMonMsg__Stats * stats)
{
	INCT();
	printf("%sNumber of portals       : %d\n", PRTT(), (int) stats->n_portals);
	printf("%sNumber of other gateways: %d\n", PRTT(), (int) stats->n_other_gateways);
	if (stats->local != NULL) {
		printf("%s----Local statistics----\n", PRTT());
		tnwg_show_lbmmon_stats_localstats(stats->local);
	}
	if (stats->portals != NULL) {
		printf("%s----Portal statistics----\n", PRTT());
		tnwg_show_lbmmon_stats_portalstatsblocks(stats->portals, stats->n_portals);
	}
	if (stats->other_gateways != NULL) {
		printf("%s----Other gateway statistics----\n", PRTT());
		tnwg_show_lbmmon_stats_othergwstatsblock(stats->other_gateways, stats->n_other_gateways);
	}
	DECT();
}

void tnwg_show_lbmmon_portals(Lbmmon__DROMonMsg__Configs__Portal *portals)
{
	INCT();
	printf("%sPortal configuration %d (%s)\n", PRTT(), portals->portal_index, (portals->portal_type == TNWG_DSTAT_Portal_Type_Peer) ? "Peer" : "Endpoint");
	printf("%sPortal name = %s\n", PRTT(), portals->portal_name);
	DECT();
}

void tnwg_show_lbmmon_configs(Lbmmon__DROMonMsg__Configs *configs)
{
	if (configs->gateway != NULL) {
		printf("%s\n", configs->gateway->config_data);
	}
	if (configs->n_portals > 0) {
		int p = 0;
		for (p = 0; p < configs->n_portals; p++) {
			tnwg_show_lbmmon_portals(configs->portals[p]);
		}
	}
}

void gateway_statistics_cb(const void * AttributeBlock, const Lbmmon__DROMonMsg * gateway_msg, void * ClientData)
{
	numtabs = 0;
	if (gateway_msg->configs != NULL) {
		print_attributes("Gateway configuration received", AttributeBlock, 0);
		tnwg_show_lbmmon_configs(gateway_msg->configs);
	}
	if (gateway_msg->stats != NULL) {
		print_attributes("Gateway statistics received", AttributeBlock, 0);
		tnwg_show_lbmmon_stats(gateway_msg->stats);
	}
	fflush(stdout);
}

/* Example of how to deserialize a packet returned from the passthrough callback */
void passthrough_statistics_cb(const lbmmon_packet_hdr_t * PacketHeader, lbmmon_packet_attributes_t * Attributes, void * AttributeBlock, void * Statistics, size_t Length, void * ClientData)
{
	const lbmmon_format_func_t * format = NULL;
	void * format_data;
	lbm_src_transport_stats_t src_stats;
	lbm_rcv_transport_stats_t rcv_stats;
	lbm_event_queue_stats_t evq_stats;
	lbm_context_stats_t ctx_stats;
	lbm_rcv_topic_stats_t * rcv_topic_stats = NULL;
	size_t rcv_topic_stats_count = 0;
	lbm_wildcard_rcv_stats_t wrcv_stats;
	Lbmmon__UMPMonMsg * store_lbmmon_msg = NULL;
	Lbmmon__DROMonMsg * gateway_lbmmon_msg = NULL;
	unsigned short module_id;
	int rc = 0;

	module_id = Attributes->mModuleID;
	if (MODULE_ID(module_id) == LBMMON_FORMAT_PB_MODULE_ID) {
		format = pb_format;
		format_data = pb_format_data;
	} else if (MODULE_ID(module_id) == LBMMON_FORMAT_CSV_MODULE_ID) {
		format = csv_format;
		format_data = csv_format_data;
	}
	if (format == NULL)
	{
		printf("ERROR! Invalid Module ID %d.\n", MODULE_ID(module_id));
		return;
	}
	switch (PacketHeader->mType)
	{
	case LBMMON_PACKET_TYPE_SOURCE:
		rc = format->mSrcDeserialize(Attributes, &src_stats, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			src_statistics_cb(Attributes, &src_stats, ClientData);
		}
		break;
	case LBMMON_PACKET_TYPE_RECEIVER:
		rc = format->mRcvDeserialize(Attributes, &rcv_stats, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			rcv_statistics_cb(Attributes, &rcv_stats, ClientData);
		}
		break;
	case LBMMON_PACKET_TYPE_EVENT_QUEUE:
		rc = format->mEvqDeserialize(Attributes, &evq_stats, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			evq_statistics_cb(Attributes, &evq_stats, ClientData);
		}
		break;
	case LBMMON_PACKET_TYPE_CONTEXT:
		rc = format->mCtxDeserialize(Attributes, &ctx_stats, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			ctx_statistics_cb(Attributes, &ctx_stats, ClientData);
		}
		break;
	case LBMMON_PACKET_TYPE_RECEIVER_TOPIC:
		rcv_topic_stats_count = 32;
		rcv_topic_stats = malloc(sizeof(lbm_rcv_topic_stats_t) * rcv_topic_stats_count);
		while (1)
		{
			rc = format->mRcvTopicDeserialize(Attributes, &rcv_topic_stats_count, rcv_topic_stats, Statistics, Length, module_id, format_data);
			if (rc == LBMMON_FORMAT_DESERIALIZE_TOO_SMALL)
			{
				rcv_topic_stats_count *= 2;
				rcv_topic_stats = realloc(rcv_topic_stats, sizeof(lbm_rcv_topic_stats_t) * rcv_topic_stats_count);
			} else
			{
				break;
			}
		}
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK)
		{
			size_t idx;

			for (idx = 0; idx < rcv_topic_stats_count; ++idx)
			{
				rcv_topic_statistics_cb(Attributes, &(rcv_topic_stats[idx]), ClientData);
			}
		}
		free(rcv_topic_stats);
		break;
	case LBMMON_PACKET_TYPE_WILDCARD_RECEIVER:
		rc = format->mWildcardRcvDeserialize(Attributes, &wrcv_stats, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			wildcard_receiver_statistics_cb(Attributes, &wrcv_stats, ClientData);
		}
		break;
	case LBMMON_PACKET_TYPE_UMESTORE:
		rc = format->mStoreDeserialize(Attributes, &store_lbmmon_msg, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			umestore_statistics_cb(Attributes, store_lbmmon_msg, ClientData);
			format->mStoreFreeUnpacked(store_lbmmon_msg);
		}
		break;
	case LBMMON_PACKET_TYPE_GATEWAY:
		rc = format->mGatewayDeserialize(Attributes, &gateway_lbmmon_msg, Statistics, Length, module_id, format_data);
		if (rc == LBMMON_FORMAT_DESERIALIZE_OK) {
			gateway_statistics_cb(Attributes, gateway_lbmmon_msg, ClientData);
			format->mGatewayFreeUnpacked(gateway_lbmmon_msg);
		}
		break;
	}
	if (rc != LBMMON_FORMAT_DESERIALIZE_OK) {
		printf("ERROR!  Deserialize function returned %d, %s\n", rc, (char *)format->mErrorMessage());
		return;
	}
}


int main(int argc, char **argv)
{
	lbmmon_rctl_t * monctl;
	lbmmon_rctl_attr_t * attr;
	lbmmon_rcv_statistics_func_t rcv = { rcv_statistics_cb };
	lbmmon_src_statistics_func_t src = { src_statistics_cb };
	lbmmon_evq_statistics_func_t evq = { evq_statistics_cb };
	lbmmon_ctx_statistics_func_t ctx = { ctx_statistics_cb };
	lbmmon_rcv_topic_statistics_func_t rcv_topic = { rcv_topic_statistics_cb };
	lbmmon_wildcard_rcv_statistics_func_t wrcv = { wildcard_receiver_statistics_cb };
	lbmmon_umestore_statistics_func_t store = { umestore_statistics_cb };
	lbmmon_gateway_statistics_func_t gateway = { gateway_statistics_cb };
	lbmmon_passthrough_statistics_func_t passthrough = { passthrough_statistics_cb };
	int rc;
	int c;
	int errflag = 0;
	char * transport_options = NULL;
	char transport_options_string[1024];
	void * format_options = NULL;
	char format_options_string[1024];
	const lbmmon_transport_func_t * transport = lbmmon_transport_lbm_module();
	const lbmmon_format_func_t * format = lbmmon_format_csv_module();

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

	lbm_log(log_callback, NULL);

	memset(transport_options_string, 0, sizeof(transport_options_string));
	memset(format_options_string, 0, sizeof(format_options_string));

	/* The following are used in the passthrough callback */
	csv_format = lbmmon_format_csv_module();
	pb_format = lbmmon_format_pb_module();
	csv_format->mInit(&csv_format_data, format_options);
	pb_format->mInit(&pb_format_data, format_options);

	while ((c = getopt_long(argc, argv, OptionString, OptionTable, NULL)) != EOF)
	{
		switch (c)
		{
			case 'c':
				/* Initialize configuration parameters from a file. */
				if (lbm_config(optarg) == LBM_FAILURE) {
					fprintf(stderr, "lbm_config: %s\n", lbm_errmsg());
					exit(1);
				}
				break;
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
					else if (strcasecmp(optarg, "lbmsnmp") == 0)
					{
						transport = lbmmon_transport_lbmsnmp_module();
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

			case 0:
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
					else if (strcasecmp(optarg, "pb") == 0)
					{
						format = lbmmon_format_pb_module();
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

			case 1:
				if (optarg != NULL)
				{
					strncpy(format_options_string, optarg, sizeof(format_options_string));
				}
				else
				{
					++errflag;
				}
				break;

			case 'h':
				fprintf(stderr, "%s\n%s\n%s\n%s",
					argv[0], lbm_version(), purpose, usage);
				exit(0);

			case '?':
			default:
				++errflag;
				break;
		}
	}

	if (errflag != 0)
	{
		fprintf(stderr, "%s\n%s\n%s",
			argv[0], lbm_version(), usage);
		exit(1);
	}

	if (strlen(transport_options_string) > 0)
	{
		transport_options = transport_options_string;
	}
	if (strlen(format_options_string) > 0)
	{
		format_options = (void *)format_options_string;
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
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_RECEIVER_TOPIC_CALLBACK, (void *)&rcv_topic, sizeof(rcv_topic));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_WILDCARD_RECEIVER_CALLBACK, (void *)&wrcv, sizeof(wrcv));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_UMESTORE_CALLBACK, (void *)&store, sizeof(store));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_GATEWAY_CALLBACK, (void *)&gateway, sizeof(gateway));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_attr_setopt(attr, LBMMON_RCTL_PASSTHROUGH_CALLBACK, (void *)&passthrough, sizeof(passthrough));
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_attr_setopt() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	rc = lbmmon_rctl_create(&monctl, format, format_options, transport, (void *)transport_options, attr, NULL);
	if (rc != 0)
	{
		fprintf(stderr, "call to lbmmon_rctl_create() failed, %s\n", lbmmon_errmsg());
		exit(1);
	}
	lbmmon_rctl_attr_delete(attr);

	while (1)
	{
		SLEEP_SEC(2);
	}
	csv_format->mFinish(&csv_format_data);
	pb_format->mFinish(&pb_format_data);

	lbmmon_rctl_destroy(monctl);
	return (0);
}

int log_callback(int Level, const char * Message, void * ClientData)
{
	fprintf(stderr, "%s\n", Message);
	return (0);
}

