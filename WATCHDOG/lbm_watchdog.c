/*
"lbmtrreq_defrib.c: application that invokes the Topic Resolution Request API after heartbeat timeout.

  Copyright (c) 2005-2018 Informatica Corporation  Permission is granted to licensees to use
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
	#include <sys/timeb.h>
	#define strcasecmp stricmp
#else
	#include <unistd.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <signal.h>
	#include <sys/time.h>
	#if defined(__TANDEM)
		#include <strings.h>
	#endif
#endif
#include "replgetopt.h"
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include "lbm-example-util.h"
#include "local_lbm.h"

#define TIME_NOT_SET ((lbm_ulong_t)-1)

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

#define TS_DATESTR_SZ 64

const char Purpose[] = "Purpose: Invokes the Topic Resolution Request API after heartbeat timeout.";
const char Usage[] =
"Usage: %s [at least one option]\n"
"Available options:\n"
"  -c, --config=FILE     Use LBM configuration file FILE.\n"
"                        Multiple config files are allowed.\n"
"                        Example:  '-c file1.cfg -c file2.cfg'\n"
"  -a, --adverts         Request Advertisements (default)\n"
"  -q, --queries         Request Queries (default)\n"
"  -w, --wildcard        Request Wildcard Queries (default)\n"
"  -A, --ctx-ads         Request Context Advertisements (default)\n"
"  -Q, --ctx-queries     Request Context Queries (default)\n"
"  -I, --gw-interest     Request Gateway Interest (default)\n"
"  -i, --interval=NUM    Interval between request (default)\n"
"  -d, --duration=NUM    Minimum duration of requests \n"
"  -n, --no_resp=NUM     Warn if there is no response to NUM queries (only valid with -R option, default=10) \n"
"  -L, --linger=NUM      Linger for NUM seconds after the set of TR requests are sent (default-10)  \n"
"  -R, --rcv-eos='topic'     Execute TR-request if there is no heart-beat 'topic' BOS after the last EOS + configured sustain-phase TQR timeout \n"
"  -M, --msg-timeout=NUM     Execute TR-request if receiver does not get a message with NUM timeout secs \n"
"  -S, --src-tr-timeout=NUM  Execute TR-request if there is no SRC context TR activity after NUM secs\n"
"  -W, --wrcv-eos='pattern'  Execute TR-request if no heart-beat BOS after 'pattern' EOS + TQR timeout \n"
"  -D, --disable-tr-request  Disable TR-requests from being executed \n"
"  -v, --verbose             Be verbose about incoming messages (-v -v  = be very verbose)\n"
"  -Y, --xml_appname        specify the application name \n"
"  -Z, --xml_config=FILE    Use LBM configuration XML FILE.\n"

;

const char * OptionString = "c:i:d:DaqwAQIL:R:S:M:vW:n:Y:Z:";

const struct option OptionTable[] =
{
	{ "config", required_argument, NULL, 'c' },
	{ "interval", required_argument, NULL, 'i' },
	{ "duration", required_argument, NULL, 'd' },
	{ "disable-tr-request", no_argument, NULL, 'D' },
	{ "linger", required_argument, NULL, 'L' },
	{ "no_resp", required_argument, NULL, 'n' },
	{ "adverts", no_argument, NULL, 'a' },
	{ "queries", no_argument, NULL, 'q' },
	{ "wildcard", no_argument, NULL, 'w' },
	{ "ctx-ads", no_argument, NULL, 'A' },
	{ "ctx-queries", no_argument, NULL, 'Q' },
	{ "gw-interest", no_argument, NULL, 'I' },
	{ "rcv-eos", required_argument, NULL, 'R' },
	{ "src-tr-timeout", required_argument, NULL,'S' },
	{ "msg-timeout", required_argument, NULL,'M' },
        { "verbose", no_argument, NULL, 'v' },
        { "wrcv-eos", required_argument, NULL, 'W' },
        { "xml_appname", required_argument, NULL, 'Y' },
        { "xml_config", required_argument, NULL, 'Z' },
	{ NULL, 0, NULL, 0 }
};

int msg_count = 0;
struct timeval cur_tv;  
struct timeval rcv_last_tv;  
struct timeval ctx_last_tv;  
lbm_ulong_t rcv_eos_count=0;
lbm_ulong_t tqr_sustain_duration = 0;
lbm_ulong_t no_response = 10;
char print_verbose = 0;
char fire_tr_req_flag = 1;

struct Options {
	lbm_ushort_t flags;
	lbm_ulong_t interval;
	lbm_ulong_t duration;
	lbm_ulong_t linger;
	lbm_ulong_t ctx_tr_timeout;
	lbm_ulong_t msg_timeout;
	unsigned char rcv_topic[256];
	unsigned char wrcv_pattern[256];
	unsigned char xml_config[256];
	unsigned char xml_appname[256];
};

void process_cmdline(int argc, char **argv, struct Options *opts)
{
	int c, errflag = 0;

	/* Set default option values */
	memset(opts, 0, sizeof(*opts));
	opts->flags = 0;
	opts->interval = TIME_NOT_SET;
	opts->duration = TIME_NOT_SET;
	opts->linger = TIME_NOT_SET;
	opts->ctx_tr_timeout = TIME_NOT_SET;
	opts->msg_timeout = TIME_NOT_SET;
	opts->rcv_topic[0] = '\0';
	opts->wrcv_pattern[0] = '\0';
	opts->xml_config[0] = '\0';
	opts->xml_appname[0] = '\0';

	/* Process the command line options, setting local variables with values */
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
			case 'i':
				opts->interval = atoi(optarg);
				break;
			case 'd':
				opts->duration = atoi(optarg);
				break;
			case 'L':
				opts->linger = atoi(optarg);
				break;
			case 'n':
				no_response = atoi(optarg);
				break;
			case 'a':
				opts->flags |= LBM_TOPIC_RES_REQUEST_ADVERTISEMENT;
				break;
			case 'q':
				opts->flags |= LBM_TOPIC_RES_REQUEST_QUERY;
				break;
			case 'w':
				opts->flags |= LBM_TOPIC_RES_REQUEST_WILDCARD_QUERY;
				break;
			case 'A':
				opts->flags |= LBM_TOPIC_RES_REQUEST_CONTEXT_ADVERTISEMENT;
				break;
			case 'Q':
				opts->flags |= LBM_TOPIC_RES_REQUEST_CONTEXT_QUERY;
				break;
			case 'I':
				opts->flags |= LBM_TOPIC_RES_REQUEST_GW_REMOTE_INTEREST;
				break;
			case 'R':
                        	if (optarg != NULL) {
                                	strncpy(opts->rcv_topic, optarg, sizeof(opts->rcv_topic));
                        	} else {
                                	errflag++;
                        	}
				break;
			case 'S':
				opts->ctx_tr_timeout = atoi(optarg);
				break;
			case 'M':
				opts->msg_timeout = atoi(optarg);
				break;
                	case 'v':
                        	print_verbose++;
                        	break;
			case 'W':
                        	if (optarg != NULL) {
                                	strncpy(opts->wrcv_pattern, optarg, sizeof(opts->wrcv_pattern));
                        	} else {
                                	errflag++;
                        	}
				break;
                	case 'D':
				fire_tr_req_flag = 0;
				fprintf(stdout, "\tchk: **********INFO! '-D' option disables TR Requests !********\n"); 
                        	break;
                	case 'Y':
                        	if (optarg != NULL) {
                                	strncpy(opts->xml_appname, optarg, sizeof(opts->xml_appname));
                        	} else {
                                	errflag++;
                        	}
                        	break;
                	case 'Z':
                        	if (optarg != NULL) {
                                	strncpy(opts->xml_config, optarg, sizeof(opts->xml_config));
                        	} else {
                                	errflag++;
                        	}
                        	break;
			default:
				errflag++;
				break;
		}

	}
	if (errflag != 0 || argc == 1)
	{
		/* An error occurred processing the command line - dump the LBM version, usage and exit */
 		fprintf(stderr, "%s\n", lbm_version());
		fprintf(stderr, Usage, argv[0]);
		exit(1);
	}
}

void print_tv(struct timeval *tv){
        /* Make buf is large enough to accomodate the string i.e buf[TS_DATESTR_SZ] */
        time_t nowtime;
        struct tm *nowtm;
        char tmbuf[TS_DATESTR_SZ];

        nowtime = tv->tv_sec;
        nowtm = localtime(&nowtime);
        strftime(tmbuf, TS_DATESTR_SZ, "%Y-%m-%d:%H.%M.%S", nowtm);
	printf("[%s.%06d]: ", tmbuf, tv->tv_usec);
}

/* Receiver callback; at EOS set the flag. Reset flag on BOS. TO DO: Actually better to use the source notification function */
int rcv_handle_msg(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
        time_t nowtime;
        struct tm *nowtm;
        char tmbuf[TS_DATESTR_SZ];

        rcv_last_tv.tv_sec = msg->tsp.tv_sec;
        rcv_last_tv.tv_usec = msg->tsp.tv_usec;

        switch (msg->type) {
        case LBM_MSG_DATA:
		msg_count++;

                if (print_verbose > 1) {
			print_tv(&rcv_last_tv);
			printf("[%s][%s], Data Message: Size:%u, Sqn:%u. Total=%u \n", msg->topic_name, msg->source, msg->len, msg->sequence_number, msg_count);
		}
                break;
        case LBM_MSG_BOS:
		print_tv(&rcv_last_tv);
		rcv_eos_count++; 
                printf("[%s][%s], Beginning of Transport Session[%u] \n", msg->topic_name, msg->source, rcv_eos_count);
                break;
        case LBM_MSG_EOS:
		print_tv(&rcv_last_tv);
                printf("[%s][%s], End of Transport Session[%u] \n", msg->topic_name, msg->source, rcv_eos_count);
		rcv_eos_count--; 
                break;
        case LBM_MSG_NO_SOURCE_NOTIFICATION:
		print_tv(&rcv_last_tv);
                printf("[%s], WARNING! No response to %u queries. \n", msg->topic_name, no_response);
                break;
        default:
                if (print_verbose) {
			print_tv(&rcv_last_tv);
                	printf("lbm_msg_t type %x [%s][%s]\n", msg->type, msg->topic_name, msg->source);
		}
                break;
        }
}


/* Logging callback */
int lbm_log_msg(int level, const char *message, void *clientd)
{
        time_t nowtime;
        struct tm *nowtm;
        char tmbuf[TS_DATESTR_SZ];

        current_tv (&cur_tv);
	print_tv (&cur_tv);
	printf("LOG Level %d: %s\n", level, message);
	return 0;
}

#define LBM_OTID_BLOCK_SZ 32
#define TS_MAX_STRING_LENGTH 1024
lbm_ulong_t resolver_events_cnt = 0;
lbm_uint32_t resolver_event_cb (lbm_context_t *ctx, int event, const void * ed, const lbm_resolver_event_info_t * info, void *clientd){
	char cnt;
	lbm_resolver_event_advertisement_t *ad= (lbm_resolver_event_advertisement_t *) ed;
	
        current_tv (&ctx_last_tv);
	resolver_events_cnt++;

        if (print_verbose > 1) {
		print_tv (&ctx_last_tv);
		printf("Resolver callback[%u]: topic[%s][%s], topic_index[%u], domainID[%u], source_domain_id[%u]", resolver_events_cnt,
			 ad->topic_string, ad->transport_string, ad->topic_index, info->domain_id, ad->source_domain_id);
		printf("\nResolver callback[%u]: opt_oid->otid = ");
		for (cnt =0; cnt < LBM_OTID_BLOCK_SZ; cnt++){
			printf("%x", ad->otid[cnt]);
		}
		{ /* Parse the OTID */
        		_lbm_transport_source_info_t tinfo;
        		char source_name[TS_MAX_STRING_LENGTH];
        		size_t source_size = sizeof(source_name);

			_lbm_otid_to_transport_source(ad->otid, &tinfo, sizeof(tinfo) );
			source_size = LBM_MSG_MAX_SOURCE_LEN;
			/* Assuming OTID specified first, otherwise should cat source_name not print it */
			cnt = _lbm_transport_source_format(&tinfo, sizeof(tinfo), source_name, &source_size);
			if(cnt != 0 ){
				printf("\n ERROR! Could not parse OTID SRC: lbm_transport_source_format=%i", cnt );
				return 10001;
			}
			printf("\nXPORT: %s[%u] OTID_SOURCE: %s \n\n",ad->transport_string, ad->topic_index, source_name);
		}
	}
} /* End resolver_event_cb() */

/*
 * Timer handler (passed into lbm_schedule_timer()) used to print TR stats
 */
#define DEFAULT_MAX_NUM_SRCS 10
lbm_rcv_transport_stats_t *stats = NULL;
int nstats; 
int timer_id = -1;

int rcv_handle_tmo(lbm_context_t *ctx, const void *clientd)
{
        int set_nstats;
	lbm_context_stats_t ctx_stats;


	lbm_context_retrieve_stats(ctx, &ctx_stats);
	printf("\nSTATS: Context TR: Sent(%u/%u), Received(%u/%u), Unresolved(%u), Rcv_topics(%u), Dropped(%u,%u,%u), Send_failed(%u)\n",
	            ctx_stats.tr_dgrams_sent, ctx_stats.tr_bytes_sent,  ctx_stats.tr_dgrams_rcved, ctx_stats.tr_bytes_rcved,
		    ctx_stats.tr_rcv_unresolved_topics, ctx_stats.tr_rcv_topics, 
		    ctx_stats.tr_dgrams_dropped_ver, ctx_stats.tr_dgrams_dropped_type, ctx_stats.tr_dgrams_dropped_malformed, ctx_stats.tr_dgrams_send_failed);

        /* Restart timer */
        if ((timer_id = lbm_schedule_timer(ctx, rcv_handle_tmo, NULL, NULL, 10000)) == -1) {
                fprintf(stderr, "lbm_schedule_timer: %s\n", lbm_errmsg());
                exit(1);
        }

        return 0;

} /* End rcv_handle_tmo() */


int main(int argc, char **argv)
{
	struct Options options,*opts = &options;
	lbm_context_t *ctx;
	lbm_context_attr_t *cattr;
	char loop_forever_flag = 0, ctx_loop_flag=0, rcv_loop_flag;
	lbm_ulong_t ctx_timeout;
	lbm_ulong_t c_sz;
	lbm_resolver_event_func_t resfunc;
        lbm_topic_t *rcv_topic; /* ptr to topic info structure for creating receiver */
        lbm_rcv_t *rcv; /* ptr to a LBM receiver object */
	lbm_rcv_topic_attr_t *rcv_attr = NULL;
	lbm_wildcard_rcv_attr_t * wrcv_attr;
	lbm_wildcard_rcv_t *wrcv;
	char fire_tr_req_flag_check = 0; 
	char * xml_config_env_check = NULL;


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
#endif /* _WIN32 */

	/* Process the different options set by the command line processing */
	process_cmdline(argc,argv,opts);
	printf("\n==============================================================\n");
 	printf("%s\n", lbm_version());
	printf("Checking TR request configuration:\n");

	if(opts->xml_config[0] != '\0'){
		/* Exit if env is set to pre-load an XML file */
		if ((xml_config_env_check = getenv("LBM_XML_CONFIG_FILENAME")) != NULL) {
			fprintf(stderr, "\n ERROR!: Please unset LBM_XML_CONFIG_FILENAME so that an XML file: can be loaded \n" );
			exit(1);
		}
		if ((xml_config_env_check = getenv("LBM_UMM_INFO")) != NULL) {
			fprintf(stderr, "\n ERROR!: Please unset LBM_UMM_INFO so that an XML file: can be loaded \n" );
			exit(1);
		}
		/* Initialize configuration parameters from a file. */
		if (lbm_config_xml_file(opts->xml_config, opts->xml_appname ) == LBM_FAILURE) {
			fprintf(stderr, "Couldn't load lbm_config_xml_file: appname: %s xml_config: %s : Error: %s\n",
				opts->xml_appname, opts->xml_config, lbm_errmsg());
			exit(1);
		}
	}
	

	if (opts->flags == 0) {
		if(  fire_tr_req_flag != 0 ){
			fprintf(stdout, "\tchk: **********WARNING! No TR Requests were specified. Requesting ALL!********\n"); 
			opts->flags = LBM_TOPIC_RES_REQUEST_QUERY | LBM_TOPIC_RES_REQUEST_CONTEXT_ADVERTISEMENT | LBM_TOPIC_RES_REQUEST_WILDCARD_QUERY
			      | LBM_TOPIC_RES_REQUEST_ADVERTISEMENT | LBM_TOPIC_RES_REQUEST_CONTEXT_QUERY | LBM_TOPIC_RES_REQUEST_GW_REMOTE_INTEREST;
		}
        }

	if (opts->interval == TIME_NOT_SET) {
		printf("\tchk: No interval given. Using 2000 milliseconds.\n");
		opts->interval = 2000;
	}

	if (opts->duration == TIME_NOT_SET) {
		printf("\tchk: No duration given. Using 5 seconds.\n");
		opts->duration = 5;
	}

	if (opts->linger == TIME_NOT_SET) {
			printf("\tchk: No linger given. Using 10 seconds.\n");
			opts->linger = 10;
	}

	printf("\tchk: Warn if no response to %u queries.\n", no_response);

	current_tv (&ctx_last_tv);
	current_tv (&rcv_last_tv);

	/* Setup logging callback */
	if (lbm_log(lbm_log_msg, NULL) == LBM_FAILURE) {
		fprintf(stderr, "lbm_log: %s \n", lbm_errmsg());
		exit(1);
	}

	/*  Retrieve default / configuration-modified context settings */
	if (lbm_context_attr_create(&cattr) == LBM_FAILURE) {
 		fprintf(stderr, "lbm_context_attr_create: %s\n", lbm_errmsg());
 		exit(1);
 	}

        {
                /* Retrieve XML modified settings for this context */
                char ctx_name[256];
                size_t ctx_name_len = sizeof(ctx_name);
                if (lbm_context_attr_str_getopt(cattr, "context_name", ctx_name, &ctx_name_len) == LBM_FAILURE) {
                        fprintf(stderr, "lbm_context_attr_str_getopt - context_name: %s\n", lbm_errmsg());
                        exit(1);
                }
                if (lbm_context_attr_set_from_xml(cattr, ctx_name) == LBM_FAILURE) {
                        fprintf(stderr, "lbm_context_attr_set_from_xml - context_name: %s\n", lbm_errmsg());
                        exit(1);
                }
        }


	if ( opts->ctx_tr_timeout != TIME_NOT_SET){
		printf("\tconfig: Creating resolver_event_function: TR Timeout set to %u seconds \n", opts->ctx_tr_timeout);
		resfunc.event_cb_func = resolver_event_cb;
		resfunc.clientd = &ctx_timeout;
		if (lbm_context_attr_setopt(cattr, "resolver_event_function", &resfunc, sizeof(resfunc) ) != 0) {
			fprintf(stderr, "lbm_context_attr_setopt:resolver_event_function: %s\n", lbm_errmsg());
			exit(1);
                }
		ctx_loop_flag = 1;
	}

	/* Create LBM context (passing in context attributes) */
	if (lbm_context_create(&ctx, cattr, NULL, NULL) == LBM_FAILURE) {
		fprintf(stderr, "lbm_context_create: %s\n", lbm_errmsg());
		exit(1);
	} else {
		printf("Context created \n");
	}
	lbm_context_attr_delete(cattr);


	if ( (opts->rcv_topic[0] != '\0') || (opts->wrcv_pattern[0] != '\0') ){ 
		lbm_ulong_t notification = 0;
		rcv_eos_count=0;
		rcv_loop_flag = 1;

        	/* init rcv topic attributes */ /* TO DO: Check if this works with xml */
        	if (lbm_rcv_topic_attr_create(&rcv_attr) == LBM_FAILURE) {
                	fprintf(stderr, "lbm_rcv_topic_attr_create: %s\n", lbm_errmsg());
                	exit(1);
        	}

		c_sz = sizeof(tqr_sustain_duration);
                if(lbm_rcv_topic_attr_getopt(rcv_attr, "resolver_query_minimum_sustain_duration", &tqr_sustain_duration, &c_sz) == LBM_FAILURE) {
                        fprintf(stderr, "lbm_rcv_topic_attr_getopt:resolver_query_minimum_sustain_duration: %s\n", lbm_errmsg());
                        exit(1);
                } 
		printf("\tconfig: resolver_query_minimum_sustain_duration = %u\n", tqr_sustain_duration);

		c_sz = sizeof(notification);
                if(lbm_rcv_topic_attr_getopt(rcv_attr, "resolution_no_source_notification_threshold", &notification, &c_sz) == LBM_FAILURE) {
                        fprintf(stderr, "lbm_rcv_topic_attr_getopt:resolution_no_source_notification_threshold: %s\n", lbm_errmsg());
                        exit(1);
                } else {
			if(notification == 0){
				printf("\tconfig: Setting resolution_no_source_notification_threshold to %u\n", no_response);
				if (lbm_rcv_topic_attr_setopt(rcv_attr, "resolution_no_source_notification_threshold", &no_response, sizeof(no_response) ) != 0) {
					fprintf(stderr, "lbm_context_attr_setopt:resolution_no_source_notification_threshold: %s\n", lbm_errmsg());
					exit(1);
				}
			} else {
				no_response = notification;
				printf("\tconfig: WARNING! resolution_no_source_notification_threshold already configured, ... ignoring '-n' option\n");
			}
                }
		if (opts->msg_timeout != TIME_NOT_SET){
			printf("\tconfig: msg_timeout = %u\n", opts->msg_timeout);
		}
	} else { 
		if (opts->msg_timeout != TIME_NOT_SET){
			printf("\tconfig: WARN: receiver(-R)/wildcard(-W) topic/pattern not provided; ignoring msg_timeout = %u\n", opts->msg_timeout);
			opts->msg_timeout = TIME_NOT_SET;
		}
	}/* End if receiver or wildcard receiver specified */


	if ( opts->rcv_topic[0] != '\0'){
		/* Look up desired topic */
        	if (lbm_rcv_topic_lookup(&rcv_topic, ctx, opts->rcv_topic, rcv_attr) == LBM_FAILURE) {
                	fprintf(stderr, "lbm_rcv_topic_lookup: %s\n", lbm_errmsg());
                	exit(1);
        	}

		printf("Creating receiver for \"%s\" \n", opts->rcv_topic );
                if (lbm_rcv_create(&rcv, ctx, rcv_topic, rcv_handle_msg, NULL, NULL)
                                == LBM_FAILURE) {
                        fprintf(stderr, "lbm_rcv_create: %s\n", lbm_errmsg());
                        exit(1);
                }
	} /* receiver create */


	if ( opts->wrcv_pattern[0] != '\0'){
		lbm_ulong_t no_source_linger;
		lbm_ulong_t wrcv_config_tqr_timeout;
		
        	/* Retrieve the current wildcard receiver attributes */
        	if (lbm_wildcard_rcv_attr_create(&wrcv_attr) == LBM_FAILURE) {
                	fprintf(stderr, "lbm_wildcard_rcv_attr_create: %s\n", lbm_errmsg());
                	exit(1);
        	}

		c_sz = sizeof(no_source_linger);
        	if (lbm_wildcard_rcv_attr_getopt(wrcv_attr, "resolver_no_source_linger_timeout",
                                                                         &no_source_linger,
                                                                         &c_sz) == LBM_FAILURE) {
                	fprintf(stderr, "lbm_wildcard_rcv_attr_getopt(pattern_type): %s\n", lbm_errmsg());
                	exit(1);
        	}
		wrcv_config_tqr_timeout = no_source_linger/1000 + 10;
		printf("\tconfig: wrcv tqr timeout(%u)  = resolver_no_source_linger_timeout(%u)ms + resolution_no_source_notification_threshold(%u)\n"
			,wrcv_config_tqr_timeout, no_source_linger, 10);
		
		
		if( tqr_sustain_duration > wrcv_config_tqr_timeout ){
			tqr_sustain_duration = wrcv_config_tqr_timeout;
		} 
		printf("\tconfig: wrcv: tqr_sustain_duration %u\n", tqr_sustain_duration);

		printf("Creating wildcard receiver for \"%s\" \n", opts->wrcv_pattern );
        	if (lbm_wildcard_rcv_create(&wrcv, ctx, opts->wrcv_pattern, rcv_attr, wrcv_attr,
                                                                rcv_handle_msg, NULL, NULL) == LBM_FAILURE) {
               	 	fprintf(stderr, "lbm_wildcard_rcv_create: %s\n", lbm_errmsg());
                	exit(1);
        	}
	} /* wildcard receiver create */


	{
		/* Print TR stats if verbose option is set */
        	if (print_verbose) {

			printf("\tconfig: Creating callback for TR statistics \n");

        		/* Start up a timer to print stats every 10 second */
        		if ((timer_id = lbm_schedule_timer(ctx, rcv_handle_tmo, NULL, NULL, 10000)) == -1) {
                		fprintf(stderr, "lbm_schedule_timer: %s\n", lbm_errmsg());
                		exit(1);
        		}
		} 
	} /* Timer for statistics */

	printf("\n==============================================================\n");
	/* rcv_eos_timeout = sustain_phase_tr_interval */
	while ((ctx_loop_flag == 1) || (rcv_loop_flag == 1)){
		char *c_tmo = "ctx_timeout";
		char c_tmo_flag;
		char *r_tmo = "rcv_timeout";
		char *m_tmo = "msg_timeout";
		char r_tmo_flag;
		char *s_tmo = NULL;
		struct timeval ctx_diff_tv;  
		struct timeval rcv_diff_tv;  
		struct timeval *diff_tv = NULL;  

		c_tmo_flag = 0; 
		r_tmo_flag = 0; 

        	current_tv (&cur_tv);
		
		if (ctx_loop_flag == 1){
                	ctx_diff_tv.tv_sec = cur_tv.tv_sec - ctx_last_tv.tv_sec;/* ideally should use a mutex */
                	ctx_diff_tv.tv_usec = cur_tv.tv_usec - ctx_last_tv.tv_usec;
                	normalize_tv(&ctx_diff_tv);
			if (ctx_loop_flag == 1 && ctx_diff_tv.tv_sec > opts->ctx_tr_timeout){
				s_tmo = c_tmo;
				c_tmo_flag = 1; 
				diff_tv = &ctx_diff_tv;
			}
		} 

		if (rcv_loop_flag == 1){
		  if (rcv_eos_count == 0) {
	               	rcv_diff_tv.tv_sec = cur_tv.tv_sec - rcv_last_tv.tv_sec;/* ideally should use a mutex */
	               	rcv_diff_tv.tv_usec = cur_tv.tv_usec - rcv_last_tv.tv_usec;
	               	normalize_tv(&rcv_diff_tv);
			if (rcv_diff_tv.tv_sec > tqr_sustain_duration){
				s_tmo = r_tmo;
				r_tmo_flag = 1; 
				diff_tv = &rcv_diff_tv;
			} else if (opts->msg_timeout != TIME_NOT_SET){
			    if (rcv_diff_tv.tv_sec > opts->msg_timeout){
				s_tmo = m_tmo;
				r_tmo_flag = 1; 
				diff_tv = &rcv_diff_tv;
			   }
		   	} 
		   }/* (rcv_eos_count == 0) */
		}/* (rcv_loop_flag == 1) */

		fire_tr_req_flag_check = fire_tr_req_flag && (c_tmo_flag || r_tmo_flag);
		if( fire_tr_req_flag_check ){
			printf("\n"), 
			print_tv (&cur_tv);
			printf("%u secs %s: Sending TR request ", diff_tv->tv_sec, s_tmo);
			if(print_verbose > 1){
				printf("( ctx(0x%x), flags(0x%x), interval(%u), duration(%u) )", 
						ctx, opts->flags, opts->interval, opts->duration);
			}
			printf("\n"), 
			lbm_context_topic_resolution_request(ctx, opts->flags, opts->interval, opts->duration);
			SLEEP_SEC(opts->linger); //Wait for things to settle 
		} else {
			SLEEP_SEC(1);
                	if (print_verbose > 1) {
				printf("\n#\n" ); 
			}
		}
		SLEEP_SEC(1);
		
		fflush (stdout);
	}/* End while */


	if (opts->linger < opts->duration) {
		printf("Linger is less than duration. Context will be deleted before duration expires.\n");
	}

	lbm_context_topic_resolution_request(ctx, opts->flags, opts->interval, opts->duration);

	if (opts->linger > 0) {
		printf("Lingering for %lu seconds...\n", opts->linger);
		SLEEP_SEC(opts->linger);
	}

	printf("Deleting context\n");
	lbm_context_delete(ctx);
	ctx = NULL;

	return 0;
}

