/*
"umpub.c: application that sends messages to a given topic (multiple sources)

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
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
#if defined(__TANDEM)
	#include <strings.h>
#if defined(HAVE_TANDEM_SPT)
	#include <ktdmtyp.h>
	#include <spthread.h>
#else
	#include <pthread.h>
#endif
#else
	#include <pthread.h>
#endif
#endif
#include "replgetopt.h"
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include "monmodopts.h"
#include "lbm-example-util.h"
#if !defined(_WIN32)
        #include <sys/utsname.h>
#endif


#define UM_PUB_VERSION "0.1"

#if defined(_MSC_VER)
#define TSTLONGLONG LONGLONG
#define TST_LARGE_INT_UNION LARGE_INTEGER
#else
#define TSTLONGLONG signed long long
typedef union {
	struct {
		unsigned long LowPart; long HighPart;
	} u;
	TSTLONGLONG QuadPart;
} TST_LARGE_INT_UNION;
#endif

/* high-res time stats at startup */
TST_LARGE_INT_UNION hrt_freq;
TST_LARGE_INT_UNION hrt_start_cnt;

#if defined(_WIN32)
#   define SLEEP_SEC(x) Sleep((x)*1000)
#   define SLEEP_MSEC(x) Sleep(x)
#else
#   define SLEEP_SEC(x) sleep(x)
#   define SLEEP_MSEC(x) \
do{\
\
if ((x) >= 1000){\
	\
		sleep((x) / 1000); \
		usleep((x) % 1000 * 1000); \
} \
else{\
		\
			usleep((x)* 1000); \
	} \
} while (0)
#endif /* _WIN32 */

#define MIN_ALLOC_MSGLEN 25
#define DEFAULT_MAX_MESSAGES 10000000
#define DEFAULT_RTT_MESSAGES 1000000
#define MAX_NUM_SRCS 100000
#define MAX_NUM_CTX 16
#define DEFAULT_NUM_SRCS 100
#define DEFAULT_NUM_CTXS 1
#define DEFAULT_NUM_THREADS 1
#define DEFAULT_MSGS_PER_SEC 10000
#define DEFAULT_TOPIC_ROOT "29west.example.multi"
#define DEFAULT_INITIAL_TOPIC_NUMBER 0
#define DEFAULT_MAX_NUM_TRANSPORTS 10
#define DEFAULT_FLIGHT_SZ -1
#define MAX_MESSAGES_INFINITE 0xEFFFFFFF

lbm_event_queue_t *evq = NULL;
lbm_src_t **srcs = NULL;
char *message = NULL;
int stats_timer_id = -1, done_sending = 0, second_timer_id;
lbm_ulong_t stats_sec = 0;

/* Performance Metrics */
int force_reclaim_total = 0;
int is_reg_complete = 1;
int msgs_stabilized = 0;
int naks_Received = 0;
double time_insecs = 0.0;
int test_active = 1;
int msgs_sent_out = 0;
int perf_msgs_sent_out = 0;
int eouldblok_sec = 0;
int msg_length = 0;
int rcv_start_thres = 0;
int rcv_start = 0;
int is_combine_test = 0;
int is_rcvapp_ready = 0;
int num_of_reg_success = 0;
int no_of_dereg_success = 0;
int num_of_reg_comp = 0;
int stop_timer = 0;
int *src_flight_size;
struct timeval *source_registration_tv;
struct timeval src_create_starttv;
struct timeval reclaim_tsp = { 0, 0 };

struct timeval starttv, curtv, endtv, msgstarttv, msgendtv;
double rtt_total = 0.0, rtt_min = 100.0, rtt_max = 0.0, rtt_median = 0.0, rtt_stddev = 0.0, rtt_avg = 0.0;
double *rtt_data = NULL;
int rtts = 0;
int rtt_min_idx = -1;
int rtt_max_idx = -1;
int msg_count = 0;

const char Purpose[] = "Purpose: Send rate-controlled messages on multiple topics.";
const char Usage[] =
"Usage: %s [options]\n"
"  Topic names generated as a root, followed by a dot, followed by an integer.\n"
"  By default, the first topic created will be '29west.example.multi.0'\n"
"Available options:\n"
"  -c, --config=FILE         Use LBM configuration file FILE.\n"
"                            Multiple config files are allowed.\n"
"                            Example:  '-c file1.cfg -c file2.cfg'\n"
"  -C  --contexts=NUM        use NUM contexts\n"
"  -d, --delay=NUM           delay sending for NUM seconds after source creation\n"
"  -f, --flight-size=NUM     allow NUM unstabilized messages in flight (determines message rate)\n"
"  -h, --help                display this help and exit\n"
"  -H, --hf                  Use hot failover sources\n"
"  -i, --initial-topic=NUM   use NUM as initial topic number [0]\n"
"  -I, --ignore=NUM          send and ignore msgs messages to warm up\n"
"  -K, --measure-latency     Calculate latency based on message payload timestamp. Use twice for round trip latency"
"  -j, --late-join=NUM       enable Late Join with specified retention buffer size (in bytes)\n"
"  -l, --length=NUM          send messages of length NUM bytes [25]\n"
"  -L, --linger=NUM          linger for NUM seconds after done [10]\n"
"  -m, --message-rate=NUM    send at NUM messages per second [10000]\n"
"  -M, --messages=NUM        send maximum of NUM messages [10000000]\n"
"  -p, --print-metrics        Print metrics to stdout every N milliseconds\n"
"  -n, --non-block           use non-blocking I/O\n"
"  -r, --root=STRING         use topic names with root of STRING [29west.example.multi]\n"
"  -R, --rate=[UM]DATA/RETR  Set transport type to LBT-R[UM], set data rate limit to\n"
"                            DATA bits per second, and set retransmit rate limit to\n"
"                            RETR bits per second.  For both limits, the optional\n"
"                            k, m, and g suffixes may be used.  For example,\n"
"                            '-R 1m/500k' is the same as '-R 1000000/500000'\n"
"  -s, --statistics=NUM      print stats every NUM seconds\n"
"  -S, --sources=NUM         use NUM sources [100]\n"
"  -t, --tight               tight loop (cpu-bound) for even message spacing\n"
"  -T, --threads=NUM         use NUM threads [1]\n"
"  -v, --verbose             be verbose about incoming messages\n"
"  -x, --bits=NUM            use NUM bits for hot failover sequence number size (32 or 64)\n"

MONOPTS_SENDER
MONMODULEOPTS_SENDER;

const char * OptionString = "c:C:Dd:hHi:I:f:j:l:L:m:M:np:r:R:s:S:tT:vx:K";
#define OPTION_MONITOR_SRC 0
#define OPTION_MONITOR_CTX 1
#define OPTION_MONITOR_TRANSPORT 2
#define OPTION_MONITOR_TRANSPORT_OPTS 3
#define OPTION_MONITOR_FORMAT 4
#define OPTION_MONITOR_FORMAT_OPTS 5
#define OPTION_MONITOR_APPID 6
const struct option OptionTable[] =
{
	{ "config", required_argument, NULL, 'c' },
	{ "contexts", required_argument, NULL, 'C' },
	{ "delay", required_argument, NULL, 'd' },
	{ "deregister", no_argument, NULL, 'D' },
	{ "help", no_argument, NULL, 'h' },
	{ "hf", no_argument, NULL, 'H' },
	{ "initial-topic", required_argument, NULL, 'i' },
	{ "ignore", required_argument, NULL, 'I' },
	{ "flight-size", required_argument, NULL, 'f' },
	{ "late-join", required_argument, NULL, 'j' },
	{ "length", required_argument, NULL, 'l' },
	{ "linger", required_argument, NULL, 'L' },
	{ "message-rate", required_argument, NULL, 'm' },
	{ "messages", required_argument, NULL, 'M' },
	{ "non-block", no_argument, NULL, 'n' },
	{ "print-metrics", required_argument, NULL, 'p' },
	{ "root", required_argument, NULL, 'r' },
	{ "rate", required_argument, NULL, 'R' },
	{ "statistics", required_argument, NULL, 's' },
	{ "sources", required_argument, NULL, 'S' },
	{ "tight", no_argument, NULL, 't' },
	{ "threads", required_argument, NULL, 'T' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "bits", required_argument, NULL, 'x' },
	{ "measure-latency", no_argument, NULL, 'K' },
	{ "monitor-src", required_argument, NULL, OPTION_MONITOR_SRC },
	{ "monitor-ctx", required_argument, NULL, OPTION_MONITOR_CTX },
	{ "monitor-transport", required_argument, NULL, OPTION_MONITOR_TRANSPORT },
	{ "monitor-transport-opts", required_argument, NULL, OPTION_MONITOR_TRANSPORT_OPTS },
	{ "monitor-format", required_argument, NULL, OPTION_MONITOR_FORMAT },
	{ "monitor-format-opts", required_argument, NULL, OPTION_MONITOR_FORMAT_OPTS },
	{ "monitor-appid", required_argument, NULL, OPTION_MONITOR_APPID },
	{ NULL, 0, NULL, 0 }
};

struct Options {
	char transport_options_string[1024];     /* Transport Options given to lbmmon_sctl_create() */
	char format_options_string[1024];        /* Format Options given to lbmmon_sctl_create() */
	char application_id_string[1024];        /* Application ID given to lbmmon_context_monitor() */
	int totalmsgsleft;                       /* Number of messages to be sent */
	size_t msglen;                           /* Length of messages to be sent */
	unsigned long int latejoin_threshold;    /* Maximum Late Join buffer size, in bytes */
	int pause;                               /* Pause interval between messages */
	int delay, linger;                       /* Interval to linger before and after sending messages */
	int block;                               /* Flag to control whether blocking sends are used	*/
	lbm_uint64_t rm_rate, rm_retrans;        /* Rate control values */
	lbm_ulong_t stats_sec;                   /* Interval for dumping statistics */
	int verifiable_msgs;                     /* Flag to control message verification (verifymsg.h) */
	int verbose;                             /* Flag to control program verbosity */
	int monitor_context;                     /* Flag to control context level monitoring */
	int monitor_context_ivl;                 /* Interval for context level monitoring */
	int monitor_source;                      /* Flag to control source level monitoring */
	int monitor_source_ivl;                  /* Interval for source level monitoring */
	lbmmon_transport_func_t * transport;     /* Function pointer to chosen transport module */
	lbmmon_format_func_t * format;           /* Function pointer to chosen format module */
	char topicroot[80];                      /* The topic to be sent on */
	int initial_topic_number;                /* Topic number to start at xxx.i */
	int msgs_per_sec;                        /* Rate to run sources at */
	int num_thrds;                           /* Number of threads to send on */
	int num_srcs;                            /* Number of soruces to send on */
	int tight_loop;                          /* Use a tight loop algorithm vs sleeping */
	char rm_protocol;                        /* LBTRM or LBTRU protocol */
	int hf;                                  /* Use Hot Failover Sources */
	int bits;                                /* HF sequence number bit size, 32 or 64 */
	int num_ctx;                             /* Number of contexts to use*/
	int deregister;                          /* deregister ump sources after sending */
	int eventq;                              /* use event queue */
	int flightsz;                            /* set flight size */
	long channel_number;                     /* The channel (sub-topic) number to use */
	int rtt;                                 /* Measure RTT if true */
	int rtt_ignore;                          /* TODO: Ignore this number of messages */
	int print_metrics;                       /* How often to print advanced metrics (milliseconds)*/
};

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

struct Options options;

void cur_usec_ofs(TSTLONGLONG *quadp)
{
	TST_LARGE_INT_UNION hrt_now_cnt;
	static TSTLONGLONG onemillion = 1000000;

#if defined(_MSC_VER)
	QueryPerformanceCounter(&hrt_now_cnt);
#else
	struct timeval now_tv;
	TSTLONGLONG perf_cnt;
	gettimeofday(&now_tv, NULL);
	perf_cnt = now_tv.tv_sec;
	perf_cnt *= 1000000;
	perf_cnt += now_tv.tv_usec;
	hrt_now_cnt.QuadPart = perf_cnt;
#endif

	*quadp = (((hrt_now_cnt.QuadPart - hrt_start_cnt.QuadPart) * onemillion) / hrt_freq.QuadPart);
}  /* cur_usec_ofs */
	
void print_latency(FILE *fp, struct timeval *tv, size_t count)
{
	double sec = 0.0, rtt = 0.0, latency = 0.0;

	sec = (double)tv->tv_sec + (double)tv->tv_usec / 1000000.0;
	printf("sec= %.04g\n", sec);
	rtt = (sec/(double)count) * 1000.0;
	latency = rtt / 2.0;
	fprintf(fp, "Elapsed time %.04g secs. %d messages (RTTs). %.04g msec RTT, %.04g msec latency\n", sec,
		(int) count, rtt, latency);
	fflush(fp);
}

void print_rtt(FILE *file, struct timeval *tsp, struct timeval *etv)
{
	double sec = 0.0;

	etv->tv_sec -= tsp->tv_sec;
	etv->tv_usec -= tsp->tv_usec;
	normalize_tv(etv);
	sec = (double)etv->tv_sec + (double)etv->tv_usec / 1000000.0;
	//printf("sec= %f\n", sec);
	rtt_total += sec;
	if (sec < rtt_min) {
		rtt_min = sec;
		rtt_min_idx = rtts;
	}
	if (sec >= rtt_max) {
		rtt_max = sec;
		rtt_max_idx = rtts;
	}
	if (rtt_data)
		rtt_data[rtts] = sec;
	rtts++;
	/* Only dump data if not collecting */
	if (options.verbose) {
		fprintf(file, "RTT %.04g msec\n", sec * 1000.0);
		fflush(file);
	}
}
	
/* Print RTT summary */
void print_rtt_results(FILE *file)
{
	if (rtt_data) {
		fprintf(file, "min/max msg = %d/%d median/stddev %.04g/%.04g msec\n",
		rtt_min_idx, rtt_max_idx, rtt_median, rtt_stddev);
	}
	fprintf(file, "%u RTT measurements. ", rtts);
	fprintf(file, "RTT min/avg/max = %.04g/%.04g/%.04g ms\n", rtt_min*1000.0, rtt_avg*1000.0, rtt_max*1000.0);
	if (rtt_max > 10) 	/* Reasonableness test  */
		fprintf(file, "Large RTT detected--perhaps you forgot the '-R' in 'lbmpong -R pong'?\n");

	fflush(file);
}

double calc_med() 
{
int r, changed;
double t;

	/* sort the result set */
	do {
		changed = 0;
		for (r = 0; r < options.totalmsgsleft - 1; r++) {
			if (rtt_data[r] > rtt_data[r + 1]) {
				t = rtt_data[r];
				rtt_data[r] = rtt_data[r + 1];
				rtt_data[r + 1] = t;
				changed = 1;
			}
		}
	} while (changed);

	if (options.totalmsgsleft & 1) {
		/* Odd number of data elements - take middle */
		return rtt_data[(options.totalmsgsleft / 2) + 1];
	}
	else {
		/* Even number of data element avg the two middle ones */
		return (rtt_data[(options.totalmsgsleft / 2)] + rtt_data[(options.totalmsgsleft / 2) + 1]) / 2;
	}
}

double calc_stddev(double mean) {
	int r;
	double sum;

	/* Subtract the mean from the data points, square them and sum them */
	sum = 0.0;
	for (r = 0; r < options.totalmsgsleft; r++) {
		rtt_data[r] -= mean;
		rtt_data[r] *= rtt_data[r];
		sum += rtt_data[r];
	}

	sum /= (options.totalmsgsleft - 1);
	return sqrt(sum);
}

void print_rtt_data(FILE *file)
{
/* If data was collected during the run, dump all the data and the idx of the min and max */
if (rtt_data)
{
	int r;

	for (r = 0; r < options.totalmsgsleft; r++)
		fprintf(file, "RTT %.04g msec, msg %d\n", rtt_data[r] * 1000.0, r);

		/* Calculate median and stddev */
		rtt_median = calc_med() * 1000.0;
		rtt_stddev = calc_stddev(rtt_avg) * 1000.0;
		print_rtt_results(file);
	}
}

/* Message handling callback (passed into lbm_rcv_create()) */
int rcv_handle_msg(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
	switch (msg->type) {
	case LBM_MSG_DATA:
		//should be if RTT
		if (options.rtt) {
			current_tv(&msgendtv);
			memcpy(&msgstarttv, msg->data, sizeof(msgstarttv));

			print_rtt(stdout, &msgstarttv, &msgendtv);
			msg_count++;
			if (msg_count == options.totalmsgsleft) {
				rtt_avg = (rtt_total / (double)rtts);

				/* Send data to stderr so it can be redirected separately */
				print_rtt_data(stderr);
				exit(0);
			}
		}
		else if (options.rtt) {
			/* Return timestamp message */
			memcpy(message, msg->data, options.msglen);
		}
		break;
	case LBM_MSG_BOS:
			printf("[%s][%s], Beginning of Transport Session\n", msg->topic_name, msg->source);
		break;
	case LBM_MSG_EOS:
		printf("[%s][%s], End of Transport Session\n", msg->topic_name, msg->source);
		break;
	case LBM_MSG_UNRECOVERABLE_LOSS:
		printf("[%s][%s][%u], LOST\n", msg->topic_name, msg->source, msg->sequence_number);
		/* Any kind of loss makes this test invalid */
		fprintf(stderr, "Unrecoverable loss occurred.  Quitting...\n");
		exit(1);
		break;
	case LBM_MSG_UNRECOVERABLE_LOSS_BURST:
		printf("[%s][%s][%u], LOST BURST\n", msg->topic_name, msg->source, msg->sequence_number);
		/* Any kind of loss makes this test invalid */
		fprintf(stderr, "Unrecoverable loss occurred.  Quitting...\n");
		exit(1);
		break;
	case LBM_MSG_UME_REGISTRATION_SUCCESS_EX:
	case LBM_MSG_UME_REGISTRATION_COMPLETE_EX:
	case LBM_MSG_UMQ_REGISTRATION_COMPLETE_EX:
		/* Provided to enable quiet usage of lbmstrm with UME */
		break;
	default:
		printf("Unknown lbm_msg_t type %x [%s][%s]\n", msg->type, msg->topic_name, msg->source);
		break;
	}
	/* LBM automatically deletes the lbm_msg_t object unless we retain it. */
	return 0;
}

int print_reg_stats()
{
	char buff[1048] = { '\0' };
	double time_in_seconds = 0.0;
	double total_time = 0.0;
	double min_time = 1000.0;
	double max_time = -1.0;
	char time_stats_buff[256];
	int i = 0;

	if (num_of_reg_success < options.num_srcs)
	{
		sprintf(time_stats_buff, "Error, not enough registrations completed\n");
	}
	else {
		/* Calculate Registration Timestamps Max/Avg */
		for (i = 0; i < options.num_srcs; i++) {
			source_registration_tv[i].tv_sec -= src_create_starttv.tv_sec;
			source_registration_tv[i].tv_usec -= src_create_starttv.tv_usec;
			normalize_tv(&source_registration_tv[i]);
			time_in_seconds = (double)source_registration_tv[i].tv_sec + (double)source_registration_tv[i].tv_usec / 1000000.0;
			if (time_in_seconds < min_time) {
				min_time = time_in_seconds;
			}
			if (time_in_seconds > max_time) {
				max_time = time_in_seconds;
			}
			total_time += time_in_seconds;
			sprintf(time_stats_buff, "Min/Avg/Max: %f/%f/%f sec", min_time, total_time / num_of_reg_comp, max_time);
		}
	}

	sprintf(buff, "-----------------------------------------------------------------------------\n"
		"Registration Statistics: "
		"Reg Success[%d] "
		"Reg Complete[%d]\n"
		"Registration time statistics: %s\n"
		"-----------------------------------------------------------------------------\n",
		num_of_reg_success,
		num_of_reg_comp,
		time_stats_buff
	);

	printf("%s", buff);
	return 0;
}

int increment_insecs(lbm_context_t *ctx, const void *clientd)
{
	lbm_context_t **ctx_array = (lbm_context_t **)clientd;

	if (test_active) //TODO: Figure out where this is set in umeperfstream
	{
		current_tv(&curtv);
		curtv.tv_sec -= starttv.tv_sec;
		curtv.tv_usec -= starttv.tv_usec;
		normalize_tv(&curtv);
		time_insecs = (double)curtv.tv_sec + (double)curtv.tv_usec / 1000000.0;
	}
	if ((second_timer_id = lbm_schedule_timer(ctx, increment_insecs, (void *)clientd, NULL, options.print_metrics)) == -1){
		fprintf(stderr, "lbm_schedule_timer: %s\n", lbm_errmsg());
		exit(1);
	}

	print_perf_stats(ctx_array);
	return 0;
}

void getTotalNaks(lbm_context_t *ctx)
{
	lbm_src_transport_stats_t stats[DEFAULT_MAX_NUM_TRANSPORTS];
	int num_transports = DEFAULT_MAX_NUM_TRANSPORTS;

	if (lbm_context_retrieve_src_transport_stats(ctx, &num_transports, stats) != LBM_FAILURE) {
		naks_Received += stats->transport.lbtrm.naks_rcved;
	}
}

int print_perf_stats(lbm_context_t **ctx_array)
{
	char buff[1048] = { '\0' };
	int temp_Average_Message = 0;
	int temp_eouldblok_sec = 0;
	static double last_time_insec = 0.0;
	static int last_perf_msgs_sent_out = 0;
	static int last_eouldblok_sec = 0;
	int i = 0;
	int total_flight = 0, max_flight = -1, min_flight = 1000000;

	if (time_insecs > 0.01)
	{
		temp_Average_Message = (int)(((double)(perf_msgs_sent_out - last_perf_msgs_sent_out))/ (time_insecs - last_time_insec));
		temp_eouldblok_sec = (int)(((double)(eouldblok_sec - last_eouldblok_sec)) / (time_insecs - last_time_insec));
	}
	else
	{
		temp_Average_Message = perf_msgs_sent_out;
		temp_eouldblok_sec = eouldblok_sec;
	}

	// Reset Counters for next iteration
	last_time_insec = time_insecs;
	last_perf_msgs_sent_out = perf_msgs_sent_out;
	last_eouldblok_sec = eouldblok_sec;
	naks_Received = 0;

	//accumulate all context nak stats
	for (i = 0; i < MAX_NUM_CTX; i++)
	{
		if (ctx_array[i] != NULL)
			getTotalNaks(ctx_array[i]);
	}

	for (i = 0; i < options.num_srcs; i++) {
		total_flight+=src_flight_size[i];
		if (src_flight_size[i] > max_flight)
			max_flight = src_flight_size[i];
		if (src_flight_size[i] < min_flight)
			min_flight = src_flight_size[i];
	}
				
	printf("Msgs/Second[%6d] Msgs Stable[%6d] Flight Size Min/Avg/Max[%4d/%4d/%4d]\n", temp_Average_Message, msgs_stabilized, min_flight,
		(total_flight / options.num_srcs), max_flight);

	if (force_reclaim_total > 0)
		printf(" Forced Reclaims[%d]", force_reclaim_total);
	if (temp_eouldblok_sec > 0)
		printf(" NAKs[%d]", temp_eouldblok_sec);
	if (naks_Received > 0)
		printf("EWOULDBLOCKS / Second[%d]", naks_Received);
}

void print_final_test_results()
{
	double stab_per = 0.0;

	printf("------------\n");
	printf("Test Results\n");
	printf("------------\n");
	printf("    Seconds Elapsed: %.04g\n", time_insecs);
	stab_per = (100 * msgs_stabilized) / perf_msgs_sent_out;
	printf("    Messages Published: %i\n", perf_msgs_sent_out);
	printf("    Stability Percentage: %.04g%%\n", stab_per);
	//print_perf_stats();
}

int handle_force_reclaim(const char *topic, lbm_uint_t sqn, void *clientd)
{
	struct timeval *tsp = (struct timeval *)clientd;
	struct timeval endtv, nowtv;
	double secs = 0;

	if (tsp == NULL) 
		fprintf(stderr, "WARNING: source for topic \"%s\" forced reclaim %x\n", topic, sqn);
	else {
		current_tv(&endtv);
		endtv.tv_sec -= tsp->tv_sec;
		endtv.tv_usec -= tsp->tv_usec;
		normalize_tv(&endtv);
		secs = (double)endtv.tv_sec + (double)endtv.tv_usec / 1000000.0;
		force_reclaim_total++;
		if (secs > 5.0) {
			fprintf(stderr, "WARNING: source for topic \"%s\" forced reclaim. Total %d.\n", topic, force_reclaim_total);
			current_tv(&nowtv);
			memcpy(tsp, &nowtv, sizeof(nowtv));
		}
	}
	return 0;
}

int handle_src_event(lbm_src_t *src, int event, void *ed, void *cd)
{
	struct Options *opts = &options;
	int src_index = *((int*)cd);
	switch (event) {
	case LBM_SRC_EVENT_CONNECT:
	{
		const char *clientname = (const char *)ed;
		printf("Receiver connect [%s]\n", clientname);
		break;
	}
	case LBM_SRC_EVENT_DISCONNECT:
	{
		const char *clientname = (const char *)ed;
		printf("Receiver disconnect [%s]\n", clientname);
		break;
	}
	case LBM_SRC_EVENT_WAKEUP:
		break;
	case LBM_SRC_EVENT_SEQUENCE_NUMBER_INFO:
	{
		lbm_src_event_sequence_number_info_t *info = (lbm_src_event_sequence_number_info_t *)ed;
		if (info->first_sequence_number != info->last_sequence_number) 
			printf("SQN [%x,%x] (cd %p)\n", info->first_sequence_number, info->last_sequence_number, (char*)(info->msg_clientd) - 1);
		else 
			printf("SQN %x (cd %p)\n", info->last_sequence_number, (char*)(info->msg_clientd) - 1);
	}
	break;
	case LBM_SRC_EVENT_UME_REGISTRATION_ERROR:
	{
		const char *errstr = (const char *)ed;
		printf("Error registering source with UME store: %s\n", errstr);
	}
	break;
	case LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS:
	{
		int i, semval;
		lbm_src_event_ume_registration_t *reg = (lbm_src_event_ume_registration_t *)ed;
	}
	break;
	case LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX:
	{
		lbm_src_event_ume_registration_ex_t *reg = (lbm_src_event_ume_registration_ex_t *)ed;
		num_of_reg_success++;
		if (opts->verbose) {
			printf("UME store %u: %s registration success. RegID %u. Flags %x ", reg->store_index, reg->store, reg->registration_id, reg->flags);
			if (reg->flags & LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX_FLAG_OLD)
				printf("OLD[SQN %x] ", reg->sequence_number);
			if (reg->flags & LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX_FLAG_NOACKS)
				printf("NOACKS ");
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_UME_DEREGISTRATION_SUCCESS_EX:
	{
		lbm_src_event_ume_registration_ex_t *reg = (lbm_src_event_ume_registration_ex_t *)ed;
		no_of_dereg_success++;
		if (opts->verbose) {
			printf("UME store %u: %s de-registration success. RegID %u. Flags %x ", reg->store_index, reg->store, reg->registration_id, reg->flags);
			if (reg->flags & LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX_FLAG_OLD)
				printf("OLD[SQN %x] ", reg->sequence_number);
			if (reg->flags & LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX_FLAG_NOACKS)
				printf("NOACKS ");
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_UME_DEREGISTRATION_COMPLETE_EX:
	{
		if (opts->verbose) 
			printf("UME DEREGISTRATION IS COMPLETE\n");
	}
	break;
	case LBM_SRC_EVENT_UME_REGISTRATION_COMPLETE_EX:
	{
		int i, semval;
		lbm_src_event_ume_registration_complete_ex_t *reg = (lbm_src_event_ume_registration_complete_ex_t *)ed;
		is_reg_complete = 0;
		num_of_reg_comp++;
		stop_timer = 1;
		current_tv(&source_registration_tv[src_index]);
		if (opts->verbose) {
			printf("UME registration complete. SQN %x. Flags %x ", reg->sequence_number, reg->flags);
			if (reg->flags & LBM_SRC_EVENT_UME_REGISTRATION_COMPLETE_EX_FLAG_QUORUM)
				printf("QUORUM ");
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_UME_STORE_UNRESPONSIVE:
	{
		const char *infostr = (const char *)ed;
		if (opts->verbose) 
			printf("UME store (STORE_UNRESPONSIVE): %s\n", infostr);
	}
	break;
	case LBM_SRC_EVENT_UME_MESSAGE_STABLE:
	{
		int i, semval;
		lbm_src_event_ume_ack_info_t *ackinfo = (lbm_src_event_ume_ack_info_t *)ed;
		if (opts->verbose)
			printf("UME message stable - sequence number %x (cd %p)\n", ackinfo->sequence_number, (char*)(ackinfo->msg_clientd) - 1);
	}
	break;
	case LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX:
	{
		int i, semval;
		lbm_src_event_ume_ack_ex_info_t *info = (lbm_src_event_ume_ack_ex_info_t *)ed;
		msgs_stabilized++;
		if (opts->verbose) {
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX_FLAG_STORE) {
			printf("UME store %u: %s message stable. SQN %x (cd %p). Flags 0x%x ", info->store_index, info->store,
			info->sequence_number, info->msg_clientd, info->flags);
			}
			else {
				printf("UME message stable. SQN %x (cd %p). Flags 0x%x ",
				info->sequence_number, info->msg_clientd, info->flags);
			}
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX_FLAG_INTRAGROUP_STABLE)
				printf("IA ");
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX_FLAG_INTERGROUP_STABLE)
				printf("IR ");
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX_FLAG_STABLE)
				printf("STABLE ");
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX_FLAG_STORE)
				printf("STORE ");
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX_FLAG_WHOLE_MESSAGE_STABLE)
				printf("MESSAGE");
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION:
	{
		lbm_src_event_ume_ack_info_t *ackinfo = (lbm_src_event_ume_ack_info_t *)ed;
		if (opts->verbose)
		printf("UME delivery confirmation - sequence number %x, Rcv RegID %u (cd %p)\n", ackinfo->sequence_number, ackinfo->rcv_registration_id, (char*)(ackinfo->msg_clientd) - 1);
	}
	break;
	case LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX:
	{
		lbm_src_event_ume_ack_ex_info_t *info = (lbm_src_event_ume_ack_ex_info_t *)ed;
		if (opts->verbose) {
			printf("UME delivery confirmation. SQN %x, RcvRegID %u (cd %p). Flags 0x%x ",
			info->sequence_number, info->rcv_registration_id, (char*)(info->msg_clientd) - 1, info->flags);
			if (info->flags & LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX_FLAG_UNIQUEACKS)
				printf("UNIQUEACKS ");
			if (info->flags & LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX_FLAG_UREGID)
				printf("UREGID ");
			if (info->flags & LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX_FLAG_OOD)
				printf("OOD ");
			if (info->flags & LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX_FLAG_EXACK)
				printf("EXACK ");
			if (info->flags & LBM_SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX_FLAG_WHOLE_MESSAGE_CONFIRMED)
				printf("MESSAGE");
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED:
	{
		lbm_src_event_ume_ack_info_t *ackinfo = (lbm_src_event_ume_ack_info_t *)ed;
		printf("UME message reclaimed - sequence number %x (cd %p)\n", ackinfo->sequence_number, (char*)(ackinfo->msg_clientd) - 1);
		}
	break;
	case LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED_EX:
	{
		lbm_src_event_ume_ack_ex_info_t *ackinfo = (lbm_src_event_ume_ack_ex_info_t *)ed;
		if (opts->verbose) {
			printf("UME message reclaimed (ex) - sequence number %x (cd %p). Flags 0x%x ",
			ackinfo->sequence_number, (char*)(ackinfo->msg_clientd) - 1, ackinfo->flags);
			if (ackinfo->flags & LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED_EX_FLAG_FORCED) 
				printf("FORCED");
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_UME_MESSAGE_NOT_STABLE:
	{
		lbm_src_event_ume_ack_ex_info_t *info = (lbm_src_event_ume_ack_ex_info_t *)ed;
		if (opts->verbose) {
			if (info->flags & LBM_SRC_EVENT_UME_MESSAGE_NOT_STABLE_FLAG_STORE) {
				printf("UME store %u: %s message NOT stable!! SQN %x (cd %p). Flags 0x%x ", info->store_index, info->store,
					info->sequence_number, info->msg_clientd, info->flags);
			}
			else {
				printf("UME message NOT stable!! SQN %x (cd %p). Flags 0x%x ",
					info->sequence_number, info->msg_clientd, info->flags);
			}
			printf("\n");
		}
	}
	break;
	case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION:
	{
		lbm_src_event_flight_size_notification_t *fsnote = (lbm_src_event_flight_size_notification_t *)ed;
		if (opts->verbose) {
			printf("Flight Size Notification. Type ");
			switch (fsnote->type) {
			case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION_TYPE_UME:
				printf("UME");
				break;
			case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION_TYPE_ULB:
				printf("ULB");
				break;
			case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION_TYPE_UMQ:
				printf("UMQ");
				break;
			default:
				printf("unknown");
				break;
			}
			printf(". Inflight is %s specified flight size\n",
			fsnote->state == LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION_STATE_OVER ? "OVER" : "UNDER");
		}
	}
	break;
	default:
		printf("Unknown source event %d\n", event);
		break;
	}
	return 0;
}
	
/* Print transport statistics */
void print_stats(FILE *fp, lbm_src_transport_stats_t *stats)
{
	fprintf(fp, "[%s]", stats->source);
	switch (stats->type) {
	case LBM_TRANSPORT_STAT_TCP:
		fprintf(fp, " buffered %lu, clients %lu\n", stats->transport.tcp.bytes_buffered,
			stats->transport.tcp.num_clients);
		break;
	case LBM_TRANSPORT_STAT_LBTRM:
		fprintf(fp, " sent %lu/%lu, txw %lu/%lu, naks %lu/%lu, ignored %lu/%lu, shed %lu, rxs %lu, rctlr %lu/%lu\n",
			stats->transport.lbtrm.msgs_sent, stats->transport.lbtrm.bytes_sent,
			stats->transport.lbtrm.txw_msgs, stats->transport.lbtrm.txw_bytes,
			stats->transport.lbtrm.naks_rcved, stats->transport.lbtrm.nak_pckts_rcved,
			stats->transport.lbtrm.naks_ignored, stats->transport.lbtrm.naks_rx_delay_ignored,
			stats->transport.lbtrm.naks_shed,
			stats->transport.lbtrm.rxs_sent,
			stats->transport.lbtrm.rctlr_data_msgs, stats->transport.lbtrm.rctlr_rx_msgs);
		break;
	case LBM_TRANSPORT_STAT_LBTRU:
		fprintf(fp, " clients %lu, sent %lu/%lu, naks %lu/%lu, ignored %lu/%lu, shed %lu, rxs %lu\n",
			stats->transport.lbtru.num_clients,
			stats->transport.lbtru.msgs_sent, stats->transport.lbtru.bytes_sent,
			stats->transport.lbtru.naks_rcved, stats->transport.lbtru.nak_pckts_rcved,
			stats->transport.lbtru.naks_ignored, stats->transport.lbtru.naks_rx_delay_ignored,
			stats->transport.lbtru.naks_shed,
			stats->transport.lbtru.rxs_sent);
		break;
	case LBM_TRANSPORT_STAT_LBTIPC:
		fprintf(fp, " clients %lu, sent %lu/%lu\n",
			stats->transport.lbtipc.num_clients,
			stats->transport.lbtipc.msgs_sent, stats->transport.lbtipc.bytes_sent);
		break;
	case LBM_TRANSPORT_STAT_LBTSMX:
		fprintf(fp, " clients %lu, sent %lu/%lu\n",
			stats->transport.lbtsmx.num_clients,
			stats->transport.lbtsmx.msgs_sent, stats->transport.lbtsmx.bytes_sent);
		break;
	case LBM_TRANSPORT_STAT_LBTRDMA:
		fprintf(fp, " clients %lu, sent %lu/%lu\n",
			stats->transport.lbtrdma.num_clients,
			stats->transport.lbtrdma.msgs_sent, stats->transport.lbtrdma.bytes_sent);
		break;
	default:
		break;
	}
	fflush(fp);
}

/* Timer callback to handle periodic display of source statistics */
int handle_stats_timer(lbm_context_t *ctx, const void *clientd)
{
	lbm_src_transport_stats_t stats[DEFAULT_MAX_NUM_TRANSPORTS];
	int num_transports = DEFAULT_MAX_NUM_TRANSPORTS;
	if (lbm_context_retrieve_src_transport_stats(ctx, &num_transports, stats) != LBM_FAILURE) {
		int scount = 0;
		
		for (scount = 0; scount < num_transports; scount++) {
			fprintf(stdout, "stats %u/%u:", scount + 1, num_transports);
			print_stats(stdout, &stats[scount]);
		}
	}
	else 
		fprintf(stderr, "lbm_context_retrieve_src_transport_stats: %s\n", lbm_errmsg());
	if (!done_sending) {
		if ((stats_timer_id = lbm_schedule_timer(ctx, handle_stats_timer, ctx, NULL, (stats_sec * 1000))) == -1) {
			fprintf(stderr, "lbm_schedule_timer: %s\n", lbm_errmsg());
			exit(1);
		}
	}
	return 0;
}

#if defined(_WIN32)
#  define MAX_NUM_THREADS 16
	int thrdidxs[MAX_NUM_THREADS];
	int msgsleft[MAX_NUM_THREADS];
#else
#  define MAX_NUM_THREADS 16
	int thrdidxs[MAX_NUM_THREADS];
	int msgsleft[MAX_NUM_THREADS];
#endif /* _WIN32 */

/*
* Per thread sending loop
*/
#if defined(_WIN32)
DWORD WINAPI sending_thread_main(void *arg)
#else
void *sending_thread_main(void *arg)
#endif /* _WIN32 */
{
	int i = 0, done = 0, block_cntr, thrdidx = *((int *)arg), rc = 0;
	lbm_uint64_t count = 0;
	lbm_src_send_ex_info_t hfexinfo;
	TSTLONGLONG msg_num = 0;
	TSTLONGLONG next_msg_usec;
	TSTLONGLONG cur_usec;
	TSTLONGLONG thrd_msgs_per_sec = (TSTLONGLONG)(options.msgs_per_sec / options.num_thrds);

	/* set up exinfo to send specified HF bit size */
	if (options.hf) {
		memset(&hfexinfo, 0, sizeof(hfexinfo));
		hfexinfo.flags = options.bits == 64 ? LBM_SRC_SEND_EX_FLAG_HF_64 : LBM_SRC_SEND_EX_FLAG_HF_32;
	}

	/*
	 * Send to each source in turn until we have sent the max number
	 * of messages total.
	 */
	block_cntr = 0;
	i = thrdidx;
	/* printf("msgs = %u\n", msgsleft[thrdidx]); */
	while (msgsleft[thrdidx] > 0 || msgsleft[thrdidx] == MAX_MESSAGES_INFINITE) {

		/* Use some (potentially idle time) to get stats */
		/* And hopefully not impact performance (much)*/
		if (options.print_metrics > 0) 
			lbm_src_get_inflight(srcs[i], LBM_FLIGHT_SIZE_TYPE_UME, &src_flight_size[i],NULL,NULL);

		cur_usec_ofs(&cur_usec);
		next_msg_usec = (msg_num * 1000000) / thrd_msgs_per_sec;
		if (options.tight_loop) {
			/* burn CPU till it's time to send one or more msgs */
			while (cur_usec < next_msg_usec) {
				cur_usec_ofs(&cur_usec);
			}
		}
		else {
			/* sleep till it's time to send one or more msgs */
			while (cur_usec < next_msg_usec) {
				SLEEP_MSEC(20);
				cur_usec_ofs(&cur_usec);
			}
		}
		
		memset(message, 0, options.msglen);
	
		if (options.rtt) {
			current_tv(&msgstarttv);
			memcpy(message, &(msgstarttv.tv_sec), sizeof(msgstarttv.tv_sec));
			memcpy(message + sizeof(msgstarttv.tv_sec), &(msgstarttv.tv_usec), sizeof(msgstarttv.tv_usec));
		}
		else 
			sprintf(message, "message %"PRIu64" (%d)", count, i);

		if (options.hf) {
			if (options.bits == 64)
				hfexinfo.hf_sqn.u64 = count;
			else
				hfexinfo.hf_sqn.u32 = (lbm_uint32_t)count;
			rc = lbm_hf_src_send_ex(srcs[i], message, options.msglen, 0, options.block ? 0 : LBM_SRC_NONBLOCK, &hfexinfo) == LBM_FAILURE;
			if (rc) {
				if (lbm_errnum() == LBM_EWOULDBLOCK) {
					block_cntr++;
					eouldblok_sec++;
				}
				else 
					fprintf(stderr, "lbm_src_send: %s\n", lbm_errmsg());
			}
			else 
				++perf_msgs_sent_out;
		}
		else {
			if (options.channel_number >= 0) {
				lbm_src_send_ex_info_t info;
				lbm_src_channel_info_t *chn = NULL;
				printf("Sending on channel %ld\n", options.channel_number); 
				if (lbm_src_channel_create(&chn, srcs[i], options.channel_number) != 0) {
					fprintf(stderr, "lbm_src_channel_create: %s\n", lbm_errmsg());
					exit(1);
				}
				memset(&info, 0, sizeof(lbm_src_send_ex_info_t));
				info.flags = LBM_SRC_SEND_EX_FLAG_CHANNEL;
				info.channel_info = chn;

				rc = lbm_src_send_ex(srcs[i], message, options.msglen, options.block ? 0 : LBM_SRC_NONBLOCK, &info);
				if (rc) {
					if (lbm_errnum() == LBM_EWOULDBLOCK) {
						block_cntr++;
						eouldblok_sec++;
					}
					else 
						fprintf(stderr, "lbm_src_send: %s\n", lbm_errmsg());
				}
				else 
					++perf_msgs_sent_out;
			}	
			else {
				rc = lbm_src_send(srcs[i], message, options.msglen, (options.block ? 0 : LBM_SRC_NONBLOCK) | LBM_MSG_FLUSH) == LBM_FAILURE;
				if (rc) {
					if (lbm_errnum() == LBM_EWOULDBLOCK) {
						block_cntr++;
						eouldblok_sec++;
					}
					else 
						fprintf(stderr, "lbm_src_send: %s\n", lbm_errmsg());
				}
				else
					++perf_msgs_sent_out;
			}
		}		
	
		if (rc == LBM_FAILURE) {
			if (lbm_errnum() == LBM_EWOULDBLOCK) {
				block_cntr++;
				if (block_cntr % 1000 == 0) 
					printf("LBM send blocked 1000 times\n");
			}
			else {
				fprintf(stderr, "lbm_src_send: %s\n", lbm_errmsg());
				exit(1);
			}
		}
	
		++msg_num;
		i += options.num_thrds;
		if (i >= options.num_srcs) {
			i = thrdidx;
			count++;
		}
		if (msgsleft[thrdidx] != MAX_MESSAGES_INFINITE) {
			if (--msgsleft[thrdidx] <= 0) {
				done = 1;
				break;
			}
		}
	
		if (done)
			break;
		}  /* while */

	return 0;
}

void process_cmdline(int argc, char **argv, struct Options *opts)
{
	int c, errflag = 0, i = 0;

	/* Print header */
        printf("*********************************************************************\n");
        printf("*\n");
        printf("*      UMTools\n");
        printf("*      umpub - Version %s\n", UM_PUB_VERSION);
        printf("*      %s\n", lbm_version());
        print_platform_info();
        printf("*\n");
        printf("*      Parameters: ");
        for (i = 0; i < argc; i++)
                printf("%s ", argv[i]);
        printf("\n*\n");
        printf("*********************************************************************\n");

	opts->initial_topic_number = DEFAULT_INITIAL_TOPIC_NUMBER;
	opts->msgs_per_sec = DEFAULT_MSGS_PER_SEC;
	opts->msglen = MIN_ALLOC_MSGLEN;
	opts->totalmsgsleft = DEFAULT_MAX_MESSAGES;
	opts->num_thrds = DEFAULT_NUM_THREADS;
	opts->num_srcs = DEFAULT_NUM_SRCS;
	opts->num_ctx = DEFAULT_NUM_CTXS;
	opts->flightsz = DEFAULT_FLIGHT_SZ;
	opts->deregister = 0;
	opts->delay = 1;
	opts->linger = 10;
	opts->eventq = 0;
	opts->block = 1;
	opts->channel_number = -1;
	opts->rtt = 0;
	opts->rtt_ignore = -1;
	opts->print_metrics = -1;

	strcpy(opts->topicroot, DEFAULT_TOPIC_ROOT);
	opts->transport = (lbmmon_transport_func_t *)lbmmon_transport_lbm_module();
	opts->format = (lbmmon_format_func_t *)lbmmon_format_csv_module();

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
		case 'C':
			opts->num_ctx = atoi(optarg);
			break;
		case 'd':
			opts->delay = atoi(optarg);
			break;
		case 'D':
			opts->deregister = 1;
			break;
		case 'f':
			opts->flightsz = atoi(optarg);
			break;
		case 'H':
			opts->hf = 1;
			break;
		case 'i':
			opts->initial_topic_number = atoi(optarg);
			break;
		case 'I':
			opts->rtt_ignore = atoi(optarg);
			break;
		case 'N':
			opts->channel_number = atol(optarg);
			break;
		case 'v':
			opts->verbose++;
			break;
		case 'j':
			if (sscanf(optarg, "%lu", &opts->latejoin_threshold) != 1)
			++errflag;
			break;
		case 'l':
			opts->msglen = atoi(optarg);
			break;
		case 'L':
			opts->linger = atoi(optarg);
			break;
		case 'm':
			opts->msgs_per_sec = atoi(optarg);
			break;
		case 'n':
			opts->block = 0;
			break;
		case 'p':
			opts->print_metrics = atoi(optarg);
			break;
		case 'M':
			if (strncmp(optarg, "I", 1) == 0)
				opts->totalmsgsleft = MAX_MESSAGES_INFINITE;
			else
				opts->totalmsgsleft = atoi(optarg);
			break;
		case 'h':
			fprintf(stderr, "%s\n%s\n", lbm_version(), Purpose);
			fprintf(stderr, Usage, argv[0]);
			exit(0);
		case 'r':
			strncpy(opts->topicroot, optarg, sizeof(opts->topicroot));
			break;
		case 'R':
			errflag += parse_rate(optarg, &opts->rm_protocol, &opts->rm_rate, &opts->rm_retrans);
			break;
		case 's':
			opts->stats_sec = atoi(optarg);
			break;
		case 'S':
			opts->num_srcs = atoi(optarg);
			if (opts->num_srcs > MAX_NUM_SRCS) {
				fprintf(stderr, "Too many sources specified. Max number of sources is %d.\n", MAX_NUM_SRCS);
				errflag++;
			}
			break;
		case 't':
			opts->tight_loop = 1;
			break;
		case 'T':
			opts->num_thrds = atoi(optarg);
			if (opts->num_thrds > MAX_NUM_THREADS) {
				fprintf(stderr, "Too many threads specified. Max number of threads is %d.\n", MAX_NUM_THREADS);
				errflag++;
			}
			break;
		case 'x':
			opts->bits = atoi(optarg);
			if (opts->bits != 32 && opts->bits != 64) {
				fprintf(stderr, "Hot failover bit size %u not 32 or 64, using 32 bit sequence numbers", opts->bits);
				opts->bits = 32;
			}
			break;
		case 'K':
			opts->rtt = 1;
			//opts->totalmsgsleft = DEFAULT_RTT_MESSAGES;
			break;
		case OPTION_MONITOR_CTX:
			opts->monitor_context = 1;
			opts->monitor_context_ivl = atoi(optarg);
			break;
		case OPTION_MONITOR_SRC:
			opts->monitor_source = 1;
			opts->monitor_source_ivl = atoi(optarg);
			break;
		case OPTION_MONITOR_TRANSPORT:
			if (optarg != NULL) {
				if (strcasecmp(optarg, "lbm") == 0) 
					opts->transport = (lbmmon_transport_func_t *)lbmmon_transport_lbm_module();
				else if (strcasecmp(optarg, "udp") == 0) 
					opts->transport = (lbmmon_transport_func_t *)lbmmon_transport_udp_module();
				else if (strcasecmp(optarg, "lbmsnmp") == 0) 
					opts->transport = (lbmmon_transport_func_t *)lbmmon_transport_lbmsnmp_module();
				else 
					++errflag;
			}
			else 
				++errflag;
			break;
		case OPTION_MONITOR_TRANSPORT_OPTS:
			if (optarg != NULL)
				strncpy(opts->transport_options_string, optarg, sizeof(opts->transport_options_string));
			else
				++errflag;
			break;
		case OPTION_MONITOR_FORMAT:
			if (optarg != NULL) {
				if (strcasecmp(optarg, "csv") == 0) 
					opts->format = (lbmmon_format_func_t *)lbmmon_format_csv_module();
				else
					++errflag;
			}
			else
				++errflag;
			break;
		case OPTION_MONITOR_FORMAT_OPTS:
			if (optarg != NULL) 
				strncpy(opts->format_options_string, optarg, sizeof(opts->format_options_string));
			else
				++errflag;
			break;
		case OPTION_MONITOR_APPID:
			if (optarg != NULL)
				strncpy(opts->application_id_string, optarg, sizeof(opts->application_id_string));
			else
				++errflag;
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

int main(int argc, char **argv)
{
	int i = 0;
	int *src_index;
	lbm_event_queue_t *evq = NULL;
	struct Options *opts = &options;
	lbm_context_t *ctx[MAX_NUM_CTX];
	lbm_topic_t *topic = NULL;
	lbm_rcv_t **rcvs = NULL;
	lbm_src_topic_attr_t * tattr;
	lbm_context_attr_t * cattr;
	char topicname[LBM_MSG_MAX_TOPIC_LEN];
	char * application_id = NULL;
	lbmmon_sctl_t * monctl[MAX_NUM_CTX];
	lbm_ume_src_force_reclaim_func_t reclaim_func;
#if defined(_WIN32)
	HANDLE wthrdh[MAX_NUM_THREADS];
	DWORD wthrdids[MAX_NUM_THREADS];
#else
	pthread_t pthids[MAX_NUM_THREADS];
#endif /* _WIN32 */

#if defined(_WIN32)
	{
		WSADATA wsadata;
		int status;
		/* Windows socket startup code */
		if ((status = WSAStartup(MAKEWORD(2, 2), &wsadata)) != 0) {
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

	/* Initialize context array to NULL */
	for (i = 0; i < MAX_NUM_CTX; i++)
		ctx[i] = NULL;

	/* Process the different options set by the command line processing */
	process_cmdline(argc, argv, opts);

	/* if message buffer is too small, then the sprintf will cause issues. So, allocate with a min size */
	if (opts->msglen < MIN_ALLOC_MSGLEN) 
		message = malloc(MIN_ALLOC_MSGLEN);
	else 
		message = malloc(opts->msglen);

	if (message == NULL) {
		fprintf(stderr, "could not allocate message buffer of size %u bytes\n", (unsigned int)opts->msglen);
		exit(1);
	}

	memset(message, 0, opts->msglen);

	if (opts->rtt) {
		/* Allocate buffer to store RTT data */
		rtt_data = (double *)malloc(opts->totalmsgsleft * sizeof(rtt_data[0]));
		printf("Latency Test. Sending %d total messages", opts->totalmsgsleft);
		/* Allocate Receiver objects */
		/* Allocate receiver array */
		if ((rcvs = malloc(sizeof(lbm_rcv_t *)* opts->num_srcs)) == NULL) {
			fprintf(stderr, "could not allocate receivers array\n");
			exit(1);
		}
	}

	printf("Target sending rate: %d msgs/sec\n", opts->msgs_per_sec);

	/* Retrieve current context settings */
	if (lbm_context_attr_create(&cattr) == LBM_FAILURE) {
		fprintf(stderr, "lbm_context_attr_create: %s\n", lbm_errmsg());
		exit(1);
	}
	/* Retrieve current source topic settings */
	if (lbm_src_topic_attr_create(&tattr) == LBM_FAILURE) {
		fprintf(stderr, "lbm_src_topic_attr_create: %s\n", lbm_errmsg());
		exit(1);
	}
	if (opts->rm_rate != 0) {
		printf("Sending with LBT-R%c data rate limit %" PRIu64 ", retransmission rate limit %" PRIu64 "\n",
		opts->rm_protocol, opts->rm_rate, opts->rm_retrans);
		/* Set transport attribute to LBT-RM */
		switch (opts->rm_protocol) {
			case 'M':
				if (lbm_src_topic_attr_str_setopt(tattr, "transport", "LBTRM") != 0) {
					fprintf(stderr, "lbm_src_topic_str_setopt:transport: %s\n", lbm_errmsg());
					exit(1);
				}
				/* Set LBT-RM data rate attribute */
				if (lbm_context_attr_setopt(cattr, "transport_lbtrm_data_rate_limit", &opts->rm_rate, sizeof(opts->rm_rate)) != 0) {
					fprintf(stderr, "lbm_context_attr_setopt:transport_lbtrm_data_rate_limit: %s\n", lbm_errmsg());
					exit(1);
				}
				/* Set LBT-RM retransmission rate attribute */
				if (lbm_context_attr_setopt(cattr, "transport_lbtrm_retransmit_rate_limit", &opts->rm_retrans, sizeof(opts->rm_retrans)) != 0) {
					fprintf(stderr, "lbm_context_attr_setopt:transport_lbtrm_retransmit_rate_limit: %s\n", lbm_errmsg());
					exit(1);
				}
				break;
			case 'U':
				if (lbm_src_topic_attr_str_setopt(tattr, "transport", "LBTRU") != 0) {
					fprintf(stderr, "lbm_src_topic_str_setopt:transport: %s\n", lbm_errmsg());
					exit(1);
				}
				/* Set LBT-RU data rate attribute */
				if (lbm_context_attr_setopt(cattr, "transport_lbtru_data_rate_limit", &opts->rm_rate, sizeof(opts->rm_rate)) != 0) {
					fprintf(stderr, "lbm_context_attr_setopt:transport_lbtru_data_rate_limit: %s\n", lbm_errmsg());
					exit(1);
				}
				/* Set LBT-RU retransmission rate attribute */
				if (lbm_context_attr_setopt(cattr, "transport_lbtru_retransmit_rate_limit", &opts->rm_retrans, sizeof(opts->rm_retrans)) != 0) {
					fprintf(stderr, "lbm_context_attr_setopt:transport_lbtru_retransmit_rate_limit: %s\n", lbm_errmsg());
					exit(1);
				}
				break;
		}
	}

	for (i = 0; i < opts->num_ctx; i++) {
		reclaim_func.func = handle_force_reclaim;
		reclaim_func.clientd = &reclaim_tsp;
		if (lbm_src_topic_attr_setopt(tattr, "ume_force_reclaim_function", &reclaim_func, sizeof(reclaim_func)) != 0) {
			fprintf(stderr, "lbm_src_topic_attr_str_setopt:ume_force_reclaim_function: %s\n", lbm_errmsg());
			exit(1);
		}

		/* Create LBM context (passing in context attributes) */
		if (lbm_context_create(&ctx[i], cattr, NULL, NULL) == LBM_FAILURE) {
			fprintf(stderr, "lbm_context_create: %s\n", lbm_errmsg());
			exit(1);
		}

		//lbm_context_attr_delete(cattr);
		if (opts->monitor_context || opts->monitor_source) {
			char * transport_options = NULL;
			char * format_options = NULL;
			if (strlen(opts->transport_options_string) > 0) 
				transport_options = opts->transport_options_string;
			if (strlen(opts->format_options_string) > 0)
				format_options = opts->format_options_string;
			if (strlen(opts->application_id_string) > 0)
				application_id = opts->application_id_string;
			if (lbmmon_sctl_create(&monctl[i], opts->format, format_options, opts->transport, transport_options) == -1) {
				fprintf(stderr, "lbmmon_sctl_create() failed, %s\n", lbmmon_errmsg());
				exit(1);
			}
		}
		if (opts->monitor_context) {
			if (lbmmon_context_monitor(monctl[i], ctx[i], application_id, opts->monitor_context_ivl) == -1) {
				fprintf(stderr, "lbmmon_context_monitor() failed, %s\n", lbmmon_errmsg());
				exit(1);
			}
		}
	}

	if (opts->latejoin_threshold > 0)
	{
		if (lbm_src_topic_attr_str_setopt(tattr, "late_join", "1") != 0) {
			fprintf(stderr, "lbm_src_topic_attr_str_setopt:late_join: %s\n", lbm_errmsg());
			exit(1);
		}
		if (lbm_src_topic_attr_setopt(tattr, "retransmit_retention_size_threshold", &opts->latejoin_threshold, sizeof(opts->latejoin_threshold)) != 0) {
			fprintf(stderr, "lbm_src_topic_attr_setopt:retransmit_retention_size_threshold: %s\n", lbm_errmsg());
			exit(1);
		}
		printf("Enabled Late Join with message retention threshold set to %lu bytes.\n", opts->latejoin_threshold);
	}

	{
		size_t flightsz_len = sizeof(opts->flightsz);
		if (opts->flightsz >= 0) {
			if (lbm_src_topic_attr_setopt(tattr, "ume_flight_size", &(opts->flightsz), sizeof(opts->flightsz)) != 0) {
				fprintf(stderr, "lbm_src_topic_attr_setopt:ume_flight_size: %s\n", lbm_errmsg());
				exit(1);
			}
		}

		if (lbm_src_topic_attr_getopt(tattr, "ume_flight_size", &(opts->flightsz), &flightsz_len) == 0)
			printf("Allowing %d in-flight message(s).\n", opts->flightsz);
		else {
			fprintf(stderr, "lbm_src_topic_attr_getopt:ume_flight_size: %s\n", lbm_errmsg());
			exit(1);
		}
	}

	if ((srcs = malloc(sizeof(lbm_src_t *)* MAX_NUM_SRCS)) == NULL) {
		fprintf(stderr, "could not allocate sources array\n");
		exit(1);
	}

	if ((src_index = (int*)malloc(sizeof(int)*opts->num_srcs)) == NULL) {
		fprintf(stderr, "could not allocate source index array\n");
		exit(1);
	}
	if (opts->print_metrics > 0) 
		if ((source_registration_tv = (struct timeval*)malloc(sizeof(struct timeval)*opts->num_srcs)) == NULL) 
			fprintf(stderr, "could not allocate registration timer memory array\n");

	if ((src_flight_size = (int*)malloc(sizeof(int)*opts->num_srcs)) == NULL) {
		fprintf(stderr, "could not source flight size array\n");
		exit(1);
	}

	/* Check if we need an eventq */
	if (opts->eventq) {
		/* Create an event queue and associate it with a callback */
		if (lbm_event_queue_create(&evq, NULL, NULL, NULL) == LBM_FAILURE) {
			fprintf(stderr, "lbm_event_queue_create: %s\n", lbm_errmsg());
			exit(1);
		}
	}

	/* create all the sources */
	printf("Creating %d sources\n", opts->num_srcs);

	/* for metrics */
	current_tv(&src_create_starttv);
	for (i = 0; i < opts->num_srcs; i++) {
		sprintf(topicname, "%s.%d", opts->topicroot, (i + opts->initial_topic_number));
		topic = NULL;
		/* First allocate the desired topic */
		if (lbm_src_topic_alloc(&topic, ctx[i%opts->num_ctx], topicname, tattr) == LBM_FAILURE) {
			fprintf(stderr, "lbm_src_topic_alloc: %s\n", lbm_errmsg());
			exit(1);
		}
		/*
		 * Create LBM source passing in the allocated topic and event
		 * handler. The source object is returned here in &(srcs[i]).
		 * The src_index is simply an integer that represents the sources uniqueness
		 * It is used for tracking stats
		 */
		src_index[i] = i;				
		if (lbm_src_create(&(srcs[i]), ctx[i%opts->num_ctx], topic, handle_src_event, &src_index[i], evq) == LBM_FAILURE) {
			fprintf(stderr, "lbm_src_create: %s\n", lbm_errmsg());
			exit(1);
		}
		src_flight_size[i] = 0;

		if (opts->monitor_source) {
			char appid[1024];
			char * appid_ptr = NULL;
			if (application_id != NULL) {
				snprintf(appid, sizeof(appid), "%s(%d)", application_id, i);
				appid_ptr = appid;
			}
			if (lbmmon_src_monitor(monctl[i%opts->num_ctx], srcs[i], appid_ptr, opts->monitor_source_ivl) == -1) {
				fprintf(stderr, "lbmmon_src_monitor() failed, %s\n", lbmmon_errmsg());
				exit(1);
			}
		}

		if (i > 1 && (i % 1000) == 0)
			printf("Created %d sources\n", i);

		if (opts->rtt) {
			lbm_topic_t *rtopic;
			char rtopicname[LBM_MSG_MAX_TOPIC_LEN];
			lbm_rcv_topic_attr_t *rcv_attr;

			if (lbm_rcv_topic_attr_create(&rcv_attr) == LBM_FAILURE) {
				fprintf(stderr, "lbm_rcv_topic_attr_create: %s\n", lbm_errmsg());
				exit(1);
			}
			sprintf(rtopicname, "%s.rtt", topicname);
	
			if (lbm_rcv_topic_lookup(&rtopic, ctx[i%opts->num_ctx], rtopicname, rcv_attr) == LBM_FAILURE) {
				fprintf(stderr, "lbm_rcv_topic_lookup: %s\n", lbm_errmsg());
				exit(1);
			}
	
			if (lbm_rcv_create(&(rcvs[i]), ctx[i%opts->num_ctx], rtopic, rcv_handle_msg, NULL, opts->eventq ? evq : NULL) == LBM_FAILURE) {
				fprintf(stderr, "lbm_rcv_create: %s\n", lbm_errmsg());
				exit(1);
			}
	
			lbm_rcv_topic_attr_delete(rcv_attr);
		}
	}
	lbm_src_topic_attr_delete(tattr);

	if (opts->delay > 0) {
		printf("Delaying for %d second%s\n", opts->delay, ((opts->delay > 1) ? "s" : ""));
		SLEEP_SEC(opts->delay);
	}
	printf("Created %d Sources. Will start sending data now.\n", opts->num_srcs);
			
	/* Start time of test.  This is used for performance metrics */
	current_tv(&starttv);

/* Get the frequency and initial count for the high-res counter */
#if defined(_MSC_VER)
	QueryPerformanceCounter(&hrt_start_cnt);
	QueryPerformanceFrequency(&hrt_freq);
#else
	{
		struct timeval now_tv;
		TSTLONGLONG perf_cnt;

		gettimeofday(&now_tv, NULL);
		perf_cnt = now_tv.tv_sec;
		perf_cnt *= 1000000;
		perf_cnt += now_tv.tv_usec;
		hrt_start_cnt.QuadPart = perf_cnt;
		hrt_freq.QuadPart = 1000000;  /* unix */
	}
#endif
	printf("Using %d threads to send.\n", opts->num_thrds);
	/* Divide sending load amongst available threads */
	for (i = 1; i < opts->num_thrds; i++) {
#if defined(_WIN32)
		thrdidxs[i] = i;
		if (opts->totalmsgsleft == MAX_MESSAGES_INFINITE) 
			msgsleft[i] = MAX_MESSAGES_INFINITE;
		else 
			msgsleft[i] = opts->totalmsgsleft / opts->num_thrds;
		if ((wthrdh[i] = CreateThread(NULL, 0, sending_thread_main, &(thrdidxs[i]), 0, &(wthrdids[i]))) == NULL) {
			fprintf(stderr, "could not create thread\n");
			exit(1);
		}
#else
		thrdidxs[i] = i;
		if (opts->totalmsgsleft == MAX_MESSAGES_INFINITE) 
			msgsleft[i] = MAX_MESSAGES_INFINITE;
		else 
			msgsleft[i] = opts->totalmsgsleft / opts->num_thrds;
		if (pthread_create(&(pthids[i]), NULL, sending_thread_main, &(thrdidxs[i])) != 0) {
			fprintf(stderr, "could not spawn thread\n");
			exit(1);
		}
#endif /* _WIN32 */
	}
	thrdidxs[0] = 0;
	if (opts->totalmsgsleft == MAX_MESSAGES_INFINITE) 
		msgsleft[0] = MAX_MESSAGES_INFINITE;
	else 
		msgsleft[0] = opts->totalmsgsleft / opts->num_thrds;

	for (i = 0; i < opts->num_ctx; i++) {
		if (opts->stats_sec > 0) {
			/* Schedule time to handle statistics display. */
			stats_sec = opts->stats_sec;
			if ((stats_timer_id = lbm_schedule_timer(ctx[i], handle_stats_timer, ctx, NULL, (stats_sec * 1000))) == -1) {
				fprintf(stderr, "lbm_schedule_timer: %s\n", lbm_errmsg());
				exit(1);
			}
		}

		/* Timer start - Using the LBM context timer scheduler for stats */
		/* The timer is created only only 1 ctx, ctx #0 */
		if ((opts->print_metrics > 0) && (i == 0)) {
			if ((second_timer_id = lbm_schedule_timer(ctx[0], increment_insecs, &ctx[0], NULL, opts->print_metrics)) == -1) {
				fprintf(stderr, "lbm_schedule_timer: %s\n", lbm_errmsg());
				exit(1);
			}
			print_reg_stats();
		}

		sending_thread_main(&(thrdidxs[0]));

		done_sending = 1;
		/* we do this before the linger, so some things may be in batching, etc. and not show up in stats. */
		//handle_stats_timer(ctx[i], ctx[i]);

		if (opts->deregister) {
			printf("Deregistering Sources...\n");
			for (i = 0; i < opts->num_srcs; ++i) {
				if ((lbm_src_ume_deregister(srcs[i])) == -1) {
					fprintf(stderr, "lbm_src_ume_deregister: %s\n", lbm_errmsg());
					exit(1);
				}
			}
		}

		if (opts->monitor_context || opts->monitor_source) {
			if (opts->monitor_context) {
				if (lbmmon_context_unmonitor(monctl[i], ctx[i]) == -1) {
					fprintf(stderr, "lbmmon_context_unmonitor() failed, %s\n", lbmmon_errmsg());
					exit(1);
				}
			}
			else {
				for (i = 0; i < opts->num_srcs; ++i) {
					if (lbmmon_src_unmonitor(monctl[i], srcs[i]) == -1) {
						fprintf(stderr, "lbmmon_src_unmonitor() failed, %s\n", lbmmon_errmsg());
						exit(1);
					}
				}
			}
			if (lbmmon_sctl_destroy(monctl[i]) == -1) {
				fprintf(stderr, "lbmmon_sctl_destoy() failed(), %s\n", lbmmon_errmsg());
				exit(1);
			}
		}
	}
	
	/*
	 * Linger allows transport to complete data transfer and recovery before
	 * context is deleted and socket is torn down.
 	 */
	printf("Lingering for %d seconds...\n", opts->linger);
	SLEEP_SEC(opts->linger);

	/* Final Test Results */
	print_final_test_results();
	/*
	 * Although not strictly necessary in this example, delete allocated
	 * sources and LBM context.
	 */
	printf("Deleting sources....\n");
	for (i = 0; i < opts->num_srcs; i++) {
		lbm_src_delete(srcs[i]);
		srcs[i] = NULL;
		if (i > 1 && (i % 1000) == 0)
			printf("Deleted %d sources\n", i);

		if (opts->rtt)
			lbm_rcv_delete(rcvs[i]);
	}

	for (i = 0; i < opts->num_ctx; i++) {
		lbm_context_delete(ctx[i]);
		ctx[i] = NULL;
	}

	if (opts->eventq) 
		lbm_event_queue_delete(evq);


	print_latency(stdout, &endtv, msg_count);
	
	printf("Quitting....\n");
	free(srcs);
	free(message);
	return 0;
}
