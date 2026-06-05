/*
  Copyright (c) 2005-2020 Informatica Corporation  Permission is granted to licensees to use
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
  
Author: Ibu Akinyemi (*) revision
*/

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#include <io.h>
	#include <direct.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>

	/* POSIX-to-Windows compatibility mappings */
	#define fseeko  _fseeki64
	#define ftello  _ftelli64
	#define fileno  _fileno
	#define access  _access
	#define unlink  _unlink
	#define F_OK    0
	/* sys/types.h (included above) already defines off_t on MSVC.
	 * Guard against redefinition to avoid C2371. */
#ifndef _OFF_T_DEFINED
	typedef __int64 off_t;
#define _OFF_T_DEFINED
#endif

	static int ftruncate(int fd, off_t length) {
		return _chsize_s(fd, length);
	}
	static int w32_mkdir(const char *path) {
		return _mkdir(path);
	}
	#define mkdir(path, mode) w32_mkdir(path)

	/* Find last path separator (handles both / and \\) */
	static const char *last_path_sep(const char *path) {
		const char *fwd = strrchr(path, '/');
		const char *bck = strrchr(path, '\\');
		if (!fwd) return bck;
		if (!bck) return fwd;
		return (fwd > bck) ? fwd : bck;
	}
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>

	/* On POSIX, path separator is always '/' */
	static const char *last_path_sep(const char *path) {
		return strrchr(path, '/');
	}
#endif
#include "replgetopt.h"
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include <lbm/umeprofile.h>
#include "lbm-example-util.h"

const char profile_repo_purpose[] = "Purpose: "
"application presents and modifies store repository state and cache file contents.\nATTENTION: Ensure umestored in NOT running!!!!"
;

const char profile_repo_usage[] =


"Usage: umesnaprepo -s state_dir [options]\n"
"Available options:\n"
"   -c, --cache-dir=PATH     cache file search PATH\n"
"** -d, --old-dir=DIR        move old state and cache files to DIR (used with -m,-P options), default is './UMDIR'\n"
"   -h, --help               display this help and exit\n"
"** -m, --move-old=TIMESTAMP move state and cache files older than TIMESTAMP to DIR directory\n"
"**                          TIMESTAMP(s): unix timestamp (e.g. 1755865293.669915) or ISO8601 UTC (e.g. 2025-07-03T15:06:00Z)\n"
"**                          Example: ./umesnaprepo -s STATE_DIR -c CACHE_DIR -d UMDIR -m0\n"
"** -l, --lastmsgonly        Dumps only the last message in cache \n"
"   -n, --no-checksum        disable cache checksum checking\n"
"   -p, --parse              enable LBM header parsing\n"
"** -P, --prune=TIMESTAMP    prune messages older than TIMESTAMP and copy original cache file to DIR directory\n"
"**                          TIMESTAMP(s): unix timestamp (e.g. 1755865293.669915) or ISO8601 UTC (e.g. 2025-07-03T15:06:00Z)\n"
"**                          Example: ./umesnaprepo -s STATE_DIR -c CACHE_DIR -d UMDIR -P`date +%s`\n"
"   -s, --state-dir=NUM      state file search PATH [required]\n"
"   -t, --truncate=NUM       limit cache message displays to NUM bytes\n"
"   -T, --terse              summarize cache and skip cache message displays\n"
;

const char * OptionString = "c:d:hlm:npP:s:t:T";
const struct option OptionTable[] =
{
	{ "cache-dir", required_argument, NULL, 'c' },
	{ "help", no_argument, NULL, 'h' },
	{ "lastmsgonly", required_argument, NULL, 'l' },
	{ "move-old", required_argument, NULL, 'm' },
	{ "no-checksum", no_argument, NULL, 'n' },
	{ "old-dir", required_argument, NULL, 'd' },
	{ "parse", no_argument, NULL, 'p' },
	{ "prune", required_argument, NULL, 'P' },
	{ "state-dir", required_argument, NULL, 's' },
	{ "truncate", required_argument, NULL, 't' },
	{ "terse", no_argument, NULL, 'T' },
	{ NULL, 0, NULL, 0 }
};

#define SR_FILENAME_MAXSIZE 1024
struct Options {
	char cache_file_search_path[SR_FILENAME_MAXSIZE];	/* search path to repository cache files */
	char state_file_search_path[SR_FILENAME_MAXSIZE];	/* search path to repository state files */
	int truncate_length;			/* truncate length of displayed message body */
	int truncate;				/* flag to control displayed message body length */
	int check_checksum;			/* flag to control cache checksum checking */
	int parse;				/* flag to control LBM header parsing */
	int skip_cache;				/* flag to control cache file searches */
	int terse;				/* flag to control cache displays */
	char move_repo_flag;			/* Flag to move older persistent files to new dir */
	char prune_repo_flag;			/* Flag to copy persisted messages to new dir and prune older messages */
	char lastmsgonly_flag;			/* Flag to dump only the last message in cache */
	time_t move_timestamp_threshold; 	/* Timestamp threshold to move older files */
	time_t prune_timestamp_threshold; 	/* Timestamp threshold to prune older files */
	char old_directory[SR_FILENAME_MAXSIZE]; 		/* move or copy state and cache files to this directory */
} profile_repo_options;
char terse_print_flag = 0;

#define TRUNCATE_MSG_LENGTH_DEFAULT 64

#define MIN(a,b) (((a) < (b)) ? (a) : (b))


#ifdef _WIN32
typedef __int64 umedisk_off_t;
#else
typedef off_t umedisk_off_t;
#endif

#pragma pack(push, 1)
typedef struct {
        lbm_uint8_t type;
        lbm_uint8_t hdr_len;
        lbm_uint16_t reserved;
        lbm_uint32_t ume_stored_version;
        umedisk_off_t start_offset;
} umedisk_rec_marker_t;
#pragma pack(pop)

#define UMEDISK_REC_MARKER_SZ (sizeof(umedisk_rec_marker_t))

/* Forward decl: defined later, called from prune_execute() */
int tailOffset(const char *cacheFile, const char *old_dir, off_t prune_offset, off_t end_record);

static const char *hexdump(const char *buffer, int size)
{
	int i, j;
	unsigned char c;
	static char hexout[256];
	char *ph = hexout;

	if(buffer == NULL){
		fprintf(stderr,"\n[ERROR]: buffer = NULL\n");
	}
	size = MIN(size, ((sizeof(hexout) / 2) - 1));

	for (i = 0; i < (size >> 4); i++) {
		for (j = 0; j < 16; j++) {
			c = buffer[(i << 4) + j];
			sprintf(ph, "%02x", c);
			ph += 2;
		}
	}
	for (i = 0; i < size % 16; i++) {
		c = buffer[size - size % 16 + i];
		sprintf(ph, "%02x", c);
		ph += 2;
	}
	return hexout;
}


/* Helper: parse ISO8601 UTC string to epoch seconds (basic) */ 
static time_t iso8601_to_epoch(const char *iso8601)
{
    struct tm tm_utc = {0};

    if (iso8601 == NULL){
	return -1;
    }

    /* parse form yyyy-mm-ddTHH:MM:SSZ (only basic support) */
    if (sscanf(iso8601, "%4d-%2d-%2dT%2d:%2d:%2dZ",
               &tm_utc.tm_year, &tm_utc.tm_mon, &tm_utc.tm_mday,
               &tm_utc.tm_hour, &tm_utc.tm_min, &tm_utc.tm_sec) != 6) {
        return (time_t)-1;
    }
    tm_utc.tm_year -= 1900;
    tm_utc.tm_mon -= 1;
    /* timegm converts tm in UTC to epoch seconds */
#ifdef _WIN32
    return _mkgmtime(&tm_utc);
#elif defined(_GNU_SOURCE) || defined(__APPLE__)
    return timegm(&tm_utc);
#else
    /* fallback: mktime assumes local time so adjust your environment carefully */
    printf("[INFO]: iso8601_to_epoch() mktime assumes local time\n");
    return mktime(&tm_utc);
#endif
}

void get_cmdlinetime(time_t *timestamp, char *optarg){

	if (strchr(optarg, 'Z')) {
 		*timestamp = iso8601_to_epoch(optarg);
	} else {
		double tmpdouble = 0.0;
		int items_scanned = sscanf(optarg, "%lf", &tmpdouble);
		if (items_scanned != 1) { 
			*timestamp = (time_t)-1;
		} else {
			*timestamp = (time_t)tmpdouble;
		}
	}
    	if (*timestamp == (time_t)-1) {
       		fprintf(stderr, "Invalid timestamp [%s] format example is 1755865293.669915 or  2025-07-03T15:06:00Z \n", optarg);
		exit(1);
	}
}/* end of get_cmdlinetime() */


int check_lockfile(const char *directory) {
    char filename[SR_FILENAME_MAXSIZE];

    // Construct full path to '.umlock' inside the given directory
    int ret = snprintf(filename, sizeof(filename), "%s/.umlock", directory);
    if (ret < 0 || ret >= (int)sizeof(filename)) {
        fprintf(stderr, "[ERROR] Directory path too long\n");
        return -1;
    }

    // Check if the file exists first
    if (access(filename, F_OK) == -1) {
        if (errno == ENOENT) {
            // File does not exist
            return 0;
        } else {
            fprintf(stderr, "[ERROR] Error checking file '%s': %s\n", filename, strerror(errno));
            return -1;
        }
    }

#ifdef _WIN32
    {
        HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "[ERROR] Failed to open file '%s': error %lu\n", filename, GetLastError());
            return -1;
        }

        OVERLAPPED ovlp = {0};
        if (!LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                        0, MAXDWORD, MAXDWORD, &ovlp)) {
            fprintf(stderr, "[ERROR] Could not lock the file '%s': error %lu\n", filename, GetLastError());
            CloseHandle(hFile);
            return -1;
        }

        /* Return a pseudo fd; caller only checks for -1 vs non-negative.
         * On Windows the handle is leaked intentionally to hold the lock. */
        return (int)(intptr_t)hFile;
    }
#else
    {
        // File exists, attempt to open for write only (do NOT create)
        int fd = open(filename, O_WRONLY);
        if (fd == -1) {
            fprintf(stderr, "[ERROR] Failed to open file '%s': %s\n", filename, strerror(errno));
            return -1;
        }

        struct flock lock_info;
        lock_info.l_type = F_WRLCK;       // Write lock
        lock_info.l_whence = SEEK_SET;    // Relative to beginning of file
        lock_info.l_start = 0;            // From start of file
        lock_info.l_len = 0;              // Lock entire file
        lock_info.l_pid = 0;              // Not used when setting lock

        if (fcntl(fd, F_SETLK, &lock_info) == -1) {
            fprintf(stderr, "[ERROR] Could not lock the file '%s': %s\n", filename, strerror(errno));
            close(fd);
            return -1;
        }

        return fd;
    }
#endif
}


void process_cmdline(int argc, char **argv, struct Options *opts)
{
	int c, errflag = 0;
	int state_path = 0;

	memset(opts, 0, sizeof(*opts));
	opts->cache_file_search_path[0] = '\0';
	opts->state_file_search_path[0] = '\0';
	opts->truncate = 0;						/* truncation disabled by default */
	opts->check_checksum = 1;					/* checksum checking enabled by default */
	opts->skip_cache = 1;						/* skip cache files by default */
	opts->truncate_length = TRUNCATE_MSG_LENGTH_DEFAULT;
	opts->move_timestamp_threshold = 0;
	opts->prune_timestamp_threshold = 0;
	opts->lastmsgonly_flag = 0;
	strncpy(opts->old_directory, "UMDIR", sizeof(opts->old_directory));



	while ((c = getopt_long(argc, argv, OptionString, OptionTable, NULL)) != EOF)
	{
		switch (c)
		{
		case 'c':
			strncpy(opts->cache_file_search_path, optarg, sizeof(opts->cache_file_search_path));
			opts->skip_cache = 0;
			break;
		case 's':
			strncpy(opts->state_file_search_path, optarg, sizeof(opts->state_file_search_path));
			state_path = 1;
			break;
		case 'h':
			fprintf(stderr, "%s\n%s\n%s\n%s", argv[0], lbm_version(), profile_repo_purpose, profile_repo_usage);
			exit(0);
		case 'm':
			opts->move_repo_flag = 1;
			get_cmdlinetime(&opts->move_timestamp_threshold, optarg); 
			if(terse_print_flag == 0) printf("[INFO]: -m: Using timestamp %lld\n", (long long)opts->move_timestamp_threshold);
			break;
		case 'd':
			strncpy(opts->old_directory, optarg, sizeof(opts->old_directory));
			break;
		case 'l':
			opts->lastmsgonly_flag = 1;
			break;
		case 'n':
			opts->check_checksum = 0;
			break;
		case 'p':
			opts->parse = 1;
			break;
		case 'P':
			opts->prune_repo_flag = 1;
			get_cmdlinetime(&opts->prune_timestamp_threshold, optarg); 
			if(terse_print_flag == 0) printf("[INFO]: -P: Using timestamp %lld\n", (long long)opts->prune_timestamp_threshold);
			break;
		case 't':
			opts->truncate_length = atoi(optarg);
			opts->truncate = 1;
			break;
		case 'T':
			opts->terse = 1;
			terse_print_flag = 1;
			break;
		default:
			errflag++;
			break;
		}
	}
	if ((errflag != 0) || !state_path) {
		fprintf(stderr, "%s\n%s\n%s", argv[0], lbm_version(), profile_repo_usage);
		exit(1);
	}
	if (opts->skip_cache) {
		if (opts->truncate) {
			fprintf(stderr, "Notice: Skipping cache files; -t option ignored\n");
		}
		if (opts->terse) {
			fprintf(stderr, "Notice: Skipping cache files; -T option ignored\n");
		}
	}
	else {
		if (opts->terse && opts->truncate) {
			fprintf(stderr, "Notice: -T option overrides truncation; -t option ignored\n");
		}
	}
	if (opts->move_repo_flag == 1 || opts->prune_repo_flag == 1){
		if( check_lockfile(opts->state_file_search_path) == -1){
			fprintf(stderr, "[ERROR] Could not access lock file. Check umestored process is NOT running and access permissions\n");
			exit(1);
		}
		if (mkdir(opts->old_directory, 0777) != 0) {
        		if (errno != EEXIST) {
            			perror("Error creating directory");
            			exit(1);
        		}
    		}
	}
}


void display_msg(char *buffer, int buffer_len, int truncate_length)
{
	int i, j, resize_len;
	unsigned char c;
	char textver[20];

	resize_len = (buffer_len > truncate_length) ? truncate_length : buffer_len;	/* Truncate long messages */
	buffer[resize_len] = 0;
	for (i = 0; i < (resize_len >> 4); i++) {
		for (j = 0; j < 16; j++) {
			c = buffer[(i << 4) + j];
			printf("%02x ", c);
			textver[j] = ((c < 0x20) || (c>0x7e)) ? '.' : c;
		}
		textver[j] = 0;
		printf("\t%s\n", textver);
	}
	for (i = 0; i < resize_len % 16; i++) {
		c = buffer[resize_len - resize_len % 16 + i];
		printf("%02x ", c);
		textver[i] = ((c < 0x20) || (c>0x7e)) ? '.' : c;
	}
	for (i = resize_len % 16; i < 16; i++) {
		printf("   ");
		textver[i] = ' ';
	}
	textver[i] = 0;
	printf("\t%s\n", textver);
	if (buffer_len > truncate_length) {
		int i = buffer_len - truncate_length;
		printf(" ...%d message byte%s truncated...\n\n", i, (i == 1)? "" : "s");
	}
}

static void print_repo_state(lbm_srp_repo_t *srp_repo, int parse, int skip_cache, int truncate_length, int terse, int lastmsgonly_flag) {
	printf("state_filename: %s\n", srp_repo->state_filename);
	printf("cache_filename: %s\n", srp_repo->cache_filename);
	printf("repo_status: %d\n", srp_repo->repo_status);
	if (srp_repo->repo_status != LBM_SRP_REPO_VALID) {
		printf("error_info: 0x%08x\n", srp_repo->error_info);
	}
	else {
		int i;
		struct in_addr sin_addr;
		unsigned d[4] = { 0, 0, 0, 0 };
		lbm_uint32_t ume_stored_version = 0;

		ume_stored_version = srp_repo->ume_stored_version;
		for (i = 0; i < 4; ++i) {
			d[i] = ume_stored_version & 0xff;
			ume_stored_version >>= 8;
		}
		printf("ume_stored_version: %d.%d.%d.%d\n", d[0], d[1], d[2], d[3]);
		printf("topicname [%s]\n", srp_repo->topicname);
		printf("transport_idx: %u\n", srp_repo->transport_idx);
		printf("topic_idx: %u\n", srp_repo->topic_idx);
		sin_addr.s_addr = srp_repo->src_addr;
		printf("src_addr: %s\n", inet_ntoa(sin_addr));
		printf("src_port: %u\n", ntohs(srp_repo->src_port));
		printf("store_id: %u\n", srp_repo->store_id);
		printf("num_rcvs: %u\n", srp_repo->num_rcvs);
		printf("num_stores: %u\n", srp_repo->num_stores);
		printf("num_grps: %u\n", srp_repo->num_grps);
		printf("sid: %llu\n", (unsigned long long)srp_repo->sid);
		printf("otid: %s\n", hexdump((char *)srp_repo->otid, LBM_OTID_BLOCK_SZ));
		printf("ctxinst: %s\n", hexdump((char *)srp_repo->ctxinst, LBM_CONTEXT_INSTANCE_BLOCK_SZ));
		printf("rpp_mode: %u\n", srp_repo->rpp_mode);
		printf("repo_type: %u\n", srp_repo->repo_type);
		printf("considered_src_activity_tmo: %lu\n", srp_repo->considered_src_activity_tmo);
		printf("considered_src_state_lifetime: %lu\n", srp_repo->considered_src_state_lifetime);
		printf("sz_threshold: %lu\n", (unsigned long)srp_repo->sz_threshold);
		printf("sz_limit: %lu\n", (unsigned long)srp_repo->sz_limit);
		printf("disk_sz_limit: %lu\n", srp_repo->disk_sz_limit);
		printf("write_delay: %u\n", srp_repo->write_delay);
		printf("src_flightsz_bytes: %llu\n", (unsigned long long)srp_repo->src_flightsz_bytes);
		printf("src_domain_id: %u\n", srp_repo->src_domain_id);
		printf("allow_ack_on_reception: %u\n", srp_repo->allow_ack_on_reception);
		printf("use_proxy: %u\n", srp_repo->use_proxy);
		if (!skip_cache) {
			printf("Repository cache:\n");
			printf("    number of messages: %d\n", srp_repo->num_msgs);
			printf("    number of duplicate messages: %d\n", srp_repo->num_msg_duplicates);
			printf("    lowest message sequence number: %d\n", srp_repo->low_sqn);
			printf("    highest message sequence number: %d\n", srp_repo->high_sqn);
			printf("    disk byte offset of the lowest sequence number: %lld\n", (long long)srp_repo->start_offset);
			printf("    disk byte offset of the highest sequence number: %lld\n", (long long)srp_repo->end_offset);
		}
		for (i = 0; i < srp_repo->num_rcvs; ++i) {
			printf("Receiver %d\n", i);
			printf("    regid: %u\n", srp_repo->rcvs[i]->regid);
			printf("    sqn: %u\n", srp_repo->rcvs[i]->sqn);
			printf("    store_id: %u\n", srp_repo->rcvs[i]->store_id);
			printf("    rcv_port: %u\n", ntohs(srp_repo->rcvs[i]->rcv_port));
			sin_addr.s_addr = srp_repo->rcvs[i]->rcv_addr;
			printf("    rcv_addr: %s\n", inet_ntoa(sin_addr));
			printf("    transport_idx: %u\n", srp_repo->rcvs[i]->transport_idx);
			printf("    topic_idx: %u\n", srp_repo->rcvs[i]->topic_idx);
			printf("    sid: %llu\n", (unsigned long long)srp_repo->rcvs[i]->sid);
			printf("    flags: 0x%02X\n", srp_repo->rcvs[i]->flags);
			printf("    otid: %s\n", hexdump((char *)srp_repo->rcvs[i]->otid, LBM_OTID_BLOCK_SZ));
			printf("    ctxinst: %s\n", hexdump((char *)srp_repo->rcvs[i]->ctxinst, LBM_CONTEXT_INSTANCE_BLOCK_SZ));
			printf("    rcv_domain_id: %u\n", srp_repo->rcvs[i]->rcv_domain_id);
			printf("    considered_activity_tmo: %lu\n", srp_repo->rcvs[i]->considered_activity_tmo);
			printf("    considered_state_lifetime: %lu\n", srp_repo->rcvs[i]->considered_state_lifetime);
		}
		if (!skip_cache && (srp_repo->num_msgs > 0) && !terse) {
			char msg_buffer[LBM_SRP_DISK_MAX_CKSUM_MSG_LEN];
			int result, sqn, start_sqn;
			lbm_srp_repo_msg_t repo_msg;

			repo_msg.buff = msg_buffer;
			if (lastmsgonly_flag == 1){
				start_sqn = srp_repo->high_sqn;
			} else {
				start_sqn = srp_repo->low_sqn;
			}
			for (sqn = start_sqn; sqn <= srp_repo->high_sqn; ++sqn) {
				result = lbm_srp_get_repo_message(srp_repo, sqn, &repo_msg);
				if (result < 0) {
					printf("Message sqn [%u] read failed; error code [%d] error info [%d]\n", sqn, srp_repo->repo_status, srp_repo->error_info);
				}
				else if (result == 0) {
					printf("Message sqn: %u was unrecoverably lost.\n", sqn);
				}
				else {
					printf("Message sqn [%u]:\n", repo_msg.sqn);
					printf("   tsp: %lld.%lld\n", (long long)repo_msg.tsp.tv_sec, (long long)repo_msg.tsp.tv_usec);
					printf("   disk_len: %lld\n", (long long)repo_msg.disk_len);
					printf("   disk_offset: %lld\n", (long long)repo_msg.disk_offset);
					printf("   flags: 0x%02x\n", repo_msg.flags);
					if (repo_msg.disk_len == 0) {
						printf("Message body: ...empty\n");
					}
					else {
						if (parse == 0) {
							printf("Message body:\n");
							display_msg(msg_buffer, repo_msg.disk_len, truncate_length);
						}
						else {
							printf("LBMC header:\n");
							printf("   ver_type: 0x%02X\n", repo_msg.lbmc_ver_type);
							printf("   next_hdr: %u\n", repo_msg.lbmc_next_hdr);
							printf("   msglen: %u\n", repo_msg.lbmc_msglen);
							printf("   tidx: %u\n", repo_msg.lbmc_tidx);
							printf("   sqn: %u\n", repo_msg.lbmc_sqn);
							if (repo_msg.fragment) {				/* this message is a fragment */
								printf("   Fragment header:\n");
								printf("      next_hdr: %u\n", repo_msg.frag_next_hdr);
								printf("      hdr_len: %u\n", repo_msg.frag_hdr_len);
								printf("      flags: 0x%04X\n", repo_msg.frag_flags);
								printf("      first_sqn: %u\n", repo_msg.frag_first_sqn);
								printf("      offset: %u\n", repo_msg.frag_offset);
								printf("      len: %u\n", ntohl(repo_msg.frag_len));
							}
							printf("Message body:\n");
							display_msg(msg_buffer + repo_msg.lbmc_msg_offset, repo_msg.disk_len - repo_msg.lbmc_msg_offset, truncate_length);
						}
					}
				}
			}
		}
		else if (skip_cache) {
			printf("***cache skipped\n");
		} else {
			if (terse) {
				printf("***cache messages skipped\n");
			}
			else {
				printf("***cache empty\n");
			}
		}
		printf("\n");
	}
}


/* Test this does not change or find some other way to find it */
/* <to do> */
typedef struct {
        lbm_uint8_t type;
        lbm_uint8_t hdr_len;
        lbm_uint16_t msg_cksum;
        lbm_uint32_t reserved; 
        struct timeval tsp;
} umedisk_rec_hdr_t;
#define UMEDISK_REC_HDR_SZ (sizeof(umedisk_rec_hdr_t))


#define BUF_SIZE (256 * 1024)    // 256KB buffer

// Keep bytes [prune_offset .. end_record]
int prune_file_inplace(const char *filepath, off_t prune_offset, off_t end_record)
{
    FILE *fp = fopen(filepath, "rb+");
    if (!fp) {
        perror("\t[ERROR]: prune_file_inplace():fopen");
        return -1;
    }
    // Get file size
    if (fseeko(fp, 0, SEEK_END) != 0) {
        perror("\t[ERROR]: prune_file_inplace():fseeko to end");
        fclose(fp);
        return -1;
    }
    off_t filesize = ftello(fp);
    if (filesize == -1) {
        perror("\t[ERROR]: prune_file_inplace():ftello");
        fclose(fp);
        return -1;
    }

    off_t src_start = prune_offset; 
    off_t dst_start = 0;                 // where remaining data should be moved to

    if (src_start > filesize) {
        // Prune offset exceeds file size after header; maybe corrupted file?
	fprintf(stderr,"\t[ERROR]: prune_file_inplace():  prune_offset greater than file size, bailing: filesize:%lld,src_start:%lld\n", (long long)filesize, (long long)src_start);
        fclose(fp);
        return 1;
    }

    if (end_record > filesize ){
	fprintf(stderr,"\t[ERROR]: prune_file_inplace(): end_record greater then file size, bailing: filesize:%lld,end_record:%lld\n", (long long)filesize, (long long)end_record);
        fclose(fp);
        return 1;
    } 


    off_t remaining = end_record - src_start;
    if(remaining <= 0){
	fprintf(stderr,"\t[ERROR]: prune_file_inplace(): unexpected wrap-around detected, bailing: end_record:%lld,prune_offset:%lld\n",(long long)end_record, (long long)prune_offset);  
        fclose(fp);
        return 1;
    } else {
        unsigned char *buf = malloc(BUF_SIZE);
        if (!buf) {
            perror("\t[ERROR]: prune_file_inplace(): malloc");
            fclose(fp);
            return -1;
        }

        // Copy remaining data forward in chunks
        // Because dst_start < src_start, copy forward from src_start to dst_start
        off_t bytes_copied = 0;
        while (bytes_copied < remaining) {
            size_t to_copy = BUF_SIZE;
            if ((remaining - bytes_copied) < BUF_SIZE) {
                to_copy = remaining - bytes_copied;
            }

            // Read chunk from src
            if (fseeko(fp, src_start + bytes_copied, SEEK_SET) != 0) {
                perror("\t[ERROR]:prune_file_inplace(): fseeko read");
                free(buf);
                fclose(fp);
                return -1;
            }

            size_t nread = fread(buf, 1, to_copy, fp);
            if (nread != to_copy) {
                if (feof(fp)) {
                    fprintf(stderr, "\t[ERROR]:prune_file_inplace(): Unexpected EOF\n");
                } else {
                    perror("\t[ERROR]:prune_file_inplace(): fread");
                }
                free(buf);
                fclose(fp);
                return -1;
            }

            /* On the last chunk, zero the end-marker's start_offset so LBM
             * knows the first valid message is now at offset 0. */
	    if((off_t)(bytes_copied + to_copy) == remaining && remaining > UMEDISK_REC_MARKER_SZ) {
		umedisk_rec_marker_t *rec_marker_pos = (umedisk_rec_marker_t *) (buf + to_copy - UMEDISK_REC_MARKER_SZ);
		if(terse_print_flag == 0) {
			printf("\t[DEBUG]: prune_file_inplace(): end-marker at prune_offset=%lld"
			       " type=%" PRIu8 " hdr_len=%" PRIu8
			       " ver=%" PRIu32 " start_offset=%lld -> 0\n",
			       (long long)prune_offset,
			       rec_marker_pos->type, rec_marker_pos->hdr_len,
			       rec_marker_pos->ume_stored_version,
			       (long long)rec_marker_pos->start_offset);
		}
		rec_marker_pos->start_offset = 0;
	    } else if((off_t)(bytes_copied + to_copy) == remaining) {
		fprintf(stderr, "\t[ERROR]: prune_file_inplace(): remaining (%lld) <= UMEDISK_REC_MARKER_SZ; end-marker not updated\n",
			(long long)remaining);
	    }

            // Write chunk at dst
            if (fseeko(fp, dst_start + bytes_copied, SEEK_SET) != 0) {
                perror("[ERROR]: prune_file_inplace(): fseeko write");
                free(buf);
                fclose(fp);
                return -1;
            }

            size_t nwritten = fwrite(buf, 1, to_copy, fp);
            if (nwritten != to_copy) {
                perror("[ERROR]: prune_file_inplace(): fwrite");
                free(buf);
                fclose(fp);
                return -1;
            }

            bytes_copied += to_copy;
        }

        free(buf);
    }

    // Truncate file to new size 
    off_t newsize = remaining;
    if (ftruncate(fileno(fp), newsize) != 0) {
        perror("[ERROR]: prune_file_inplace(): ftruncate");
        fclose(fp);
        return -1;
    }


    fclose(fp);
    return 0;
}


static off_t get_file_size(const char *filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        perror("[ERROR]: get_file_size(): stat");
        return -1;
    }
    return st.st_size;
}

int moveCacheFileIdx(const char *oldCacheFile, const char *newCacheFile){
    char newCacheFileidx[SR_FILENAME_MAXSIZE]; 
    char oldCacheFileidx[SR_FILENAME_MAXSIZE]; 
    int required_len;

    // Construct the cache index file path
    required_len = snprintf(newCacheFileidx, sizeof(newCacheFileidx), "%s.idx", newCacheFile);
    if (required_len < 0 || (size_t)required_len >= sizeof(newCacheFileidx)) {
    	fprintf(stderr, "\t[ERROR]: moveCacheFileIdx() new cache file Index file path is too long %s.\n", newCacheFileidx);
    	return -1;
    }
    required_len = snprintf(oldCacheFileidx, sizeof(oldCacheFileidx), "%s.idx", oldCacheFile);
    if (required_len < 0 || (size_t)required_len >= sizeof(oldCacheFileidx)) {
    	fprintf(stderr, "\t[ERROR]: moveCacheFileIdx() Old cache file Index file path is too long %s. \n", oldCacheFileidx);
    	return -1;
    }

    if (access(oldCacheFileidx, F_OK) != -1) {
	    if (rename(oldCacheFileidx, newCacheFileidx) == 0) {
	        printf("\t[INFO] moveCacheFileIdx():File '%s' renamed and moved to '%s' successfully.\n", oldCacheFileidx, newCacheFileidx);
	    } else {
	        // Handle rename errors
	        switch (errno) {
	            case ENOENT:
	                fprintf(stderr, "\t[ERROR]:moveCacheFileIdx() Either file does not exist '%s':'%s'.\n", oldCacheFileidx, newCacheFileidx);
	                break;
	            case EACCES:
	                fprintf(stderr, "\t[ERROR]:moveCacheFileIdx() Permission denied for moving file '%s'.\n", oldCacheFileidx);
	                break;
	            case EXDEV:
	                fprintf(stderr, "\t[ERROR]:moveCacheFileIdx() Cannot move file across different filesystems.\n");
	                break;
	            default:
	                perror("\t[ERROR]:moveCacheFileIdx() renaming/moving file");
	                break;
	        }
	        return -1; // Failure
	    }
    } else {
	    /* File does not exist */
	    return 0;
    }
    return 0;
}/* End moveCacheFileIdx */


int replace_file(const char *target_file, const char *updated_file) {
    // Remove the target file first
    if (unlink(target_file) != 0) {
        if (errno != ENOENT) { // Ignore if target file does not exist
            perror("\t[ERROR]:replace_file() Error removing target file\n");
            return -1;
        }
    }
    // Rename updated_file to target_file (effectively replacing it)
    if (rename(updated_file, target_file) != 0) {
        perror("\t[ERROR]:replace_file() Error renaming updated file to target file\n");
        return -1;
    }
    printf("\t[INFO]:replace_file() Replaced '%s' with '%s' successfully.\n", target_file, updated_file);
    return 0;
}


/* Per-message metadata pre-collected from LBM before file I/O.
 * Used by filterMsgs() to separate the LBM read phase from the file I/O phase
 * so the LBM file handle can be released before the cache file is opened. */
typedef struct {
    off_t  disk_offset;
    off_t  disk_len;
    time_t tsp_sec;
    int    sqn;
    int    keep;        /* 1 = write to filtered output */
    int    status;      /* 0=ok, 1=unrecoverably lost, -1=read error */
    int    repo_status; /* srp_repo->repo_status (on read error) */
    int    error_info;  /* srp_repo->error_info (on read error) */
} filter_msg_t;

/* What file I/O action to perform after LBM handles are released */
typedef enum {
    PRUNE_ACTION_NONE   = 0, /* nothing to do */
    PRUNE_ACTION_TAIL,       /* cases 2 and 3: tailOffset() */
    PRUNE_ACTION_FILTER      /* case 4: filter by per-message entry list */
} prune_action_t;

/*
 * prune_decision_t - result of prune_collect(); consumed by prune_execute().
 * All LBM data is captured here so no LBM handle is needed during file I/O.
 */
typedef struct {
    prune_action_t  action;
    char            cache_filename[SR_FILENAME_MAXSIZE];
    char            bkup_dir[SR_FILENAME_MAXSIZE];
    /* for PRUNE_ACTION_TAIL (cases 2 and 3) */
    off_t           msg_offset;
    off_t           end_of_record;
    /* for PRUNE_ACTION_FILTER (case 4) */
    int             low_sqn;
    int             high_sqn;
    off_t           end_offset;
    filter_msg_t   *entries;   /* malloc'd; caller frees */
    int             num_entries;
} prune_decision_t;

/* Convert epoch seconds to ISO8601 time string (forward-referenced by prune_collect) */
static void epoch_to_iso8601(time_t epoch_seconds, char *buf, size_t buf_len) {
    struct tm tm_utc;
#ifdef _WIN32
    gmtime_s(&tm_utc, &epoch_seconds);
#else
    gmtime_r(&epoch_seconds, &tm_utc);
#endif
    strftime(buf, buf_len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

/*
 * prune_collect - Phase 1: query LBM to decide what pruning is needed.
 *
 * Fills *dec with the action and all parameters needed for file I/O.
 * Does NOT open or write any files. Does NOT free srp_repo.
 * main() calls lbm_srp_free_repo_state() then lbm_srp_delete() after all
 * repos are collected, then calls prune_execute() for each decision.
 *
 * Returns 0 on success (dec->action set), -1 on error.
 */
static int prune_collect(lbm_srp_repo_t *srp_repo, time_t threshold,
                          const char *bkup_dir, prune_decision_t *dec)
{
    time_t high_time;
    int sqn, result, keep_sqn;
    off_t msg_offset = 0;
    char msg_buffer[LBM_SRP_DISK_MAX_CKSUM_MSG_LEN];

    memset(dec, 0, sizeof(*dec));
    dec->action = PRUNE_ACTION_NONE;
    strncpy(dec->cache_filename, srp_repo->cache_filename, sizeof(dec->cache_filename) - 1);
    strncpy(dec->bkup_dir, bkup_dir, sizeof(dec->bkup_dir) - 1);
    dec->end_of_record = srp_repo->end_offset + UMEDISK_REC_MARKER_SZ;
    //printf(stderr, "\t[ERROR]: prune_collect() for %s and %s: srp_repo->end_offset [%d] UMEDISK_REC_MARKER_SZ [%d]", srp_repo->state_filename, srp_repo->cache_filename, srp_repo->end_offset, UMEDISK_REC_MARKER_SZ);

    lbm_srp_repo_msg_t *repo_msg = malloc(sizeof(lbm_srp_repo_msg_t));
    if (repo_msg == NULL) {
        perror("[ERROR]: prune_collect() malloc failed");
        return -1;
    }
    repo_msg->buff = msg_buffer;

    if (srp_repo->repo_status != LBM_SRP_REPO_VALID) {
        fprintf(stderr, "\t[ERROR]: prune_collect() for %s and %s:",
                srp_repo->state_filename, srp_repo->cache_filename);
        fprintf(stderr, "error_info: 0x%08x\n", srp_repo->error_info);
        free(repo_msg);
        return -1;
    }

    if (srp_repo->num_msgs == 0) {
        if (terse_print_flag == 0) fprintf(stderr, "[INFO]: prune_collect() for %s and %s:",
                                           srp_repo->state_filename, srp_repo->cache_filename);
        if (terse_print_flag == 0) fprintf(stderr, "No messages in repo\n");
        free(repo_msg);
        return 0;
    }

    printf("[INFO]: [%s]\n", srp_repo->cache_filename);

    /* Special case 1: lowest SQN timestamp >= threshold — nothing to prune */
    result = lbm_srp_get_repo_message(srp_repo, srp_repo->low_sqn, repo_msg);
    if (result == 1) {
        if (repo_msg->tsp.tv_sec >= threshold) {
            printf("\t[INFO]: prune_collect-1 %s: Low SQN# timestamp %lld >= threshold %lld: skipping.\n",
                   srp_repo->cache_filename, (long long)repo_msg->tsp.tv_sec, (long long)threshold);
            free(repo_msg);
            return 0;
        }
    } else {
        fprintf(stderr, "\t[ERROR]:prune_collect-1: result of lbm_srp_get_repo_message (%i) = %i \n",
                srp_repo->low_sqn, result);
        free(repo_msg);
        return -1;
    }

    /* Special case 2: highest timestamp <= threshold — keep only the last message */
    result = lbm_srp_get_repo_message(srp_repo, srp_repo->high_sqn, repo_msg);
    if (result == 1) {
        msg_offset = 0;
        if (repo_msg->tsp.tv_sec <= threshold) {
            printf("\t[INFO]: prune_collect-2: high SQN#[%i] timestamp[%lld] <= threshold[%lld]: keeping last message.\n",
                   srp_repo->high_sqn, (long long)repo_msg->tsp.tv_sec, (long long)threshold);
            if (srp_repo->num_msgs == 1) {
                free(repo_msg);
                return 0;
            }
            keep_sqn   = srp_repo->high_sqn;
            msg_offset = repo_msg->disk_offset;
            (void)keep_sqn; /* stored in dec->action/msg_offset below */
            free(repo_msg);
            dec->action     = PRUNE_ACTION_TAIL;
            dec->msg_offset = msg_offset;
            return 0;
        }
        /* tsp > threshold: fall through to case 3/4 with repo_msg holding high_sqn data */
    } else {
        fprintf(stderr, "\t[ERROR]:prune_collect-2: result of lbm_srp_get_repo_message (%i) = %i \n",
                srp_repo->high_sqn, result);
        free(repo_msg);
        return -1;
    }

    if (srp_repo->start_offset < srp_repo->end_offset) {
        /* Special case 3: no wraparound — shrink from the front */
        char high_ts_str3[32] = "None";
        keep_sqn  = 0;
        high_time = repo_msg->tsp.tv_sec;
        printf("\t[INFO]:prune_collect-3 low_sqn [%u] high_sqn-1 [%u] num_msgs [%u]\n",
               srp_repo->low_sqn, srp_repo->high_sqn - 1, srp_repo->num_msgs);
        for (sqn = srp_repo->low_sqn; sqn <= srp_repo->high_sqn - 1; ++sqn) {
            if (lbm_srp_get_repo_message(srp_repo, sqn, repo_msg) == 1) {
                if (repo_msg->tsp.tv_sec >= threshold) {
                    break;
                } else {
                    keep_sqn   = sqn;
                    msg_offset = repo_msg->disk_offset;
                    high_time  = repo_msg->tsp.tv_sec;
                    if (terse_print_flag == 0)
                        printf("\t[DEBUG]:sqn[%d] msg_offset[%lld] time[%lld]<[%lld]\n",
                               keep_sqn, (long long)msg_offset, (long long)high_time,
                               (long long)threshold);
                }
            }
        }
        free(repo_msg);
        if (keep_sqn == 0 || keep_sqn == srp_repo->low_sqn) {
            printf("\t[INFO]: prune_collect-3: %s: Nothing to do. Keep msg[%i]\n",
                   dec->cache_filename, keep_sqn);
            return 0;
        }
        epoch_to_iso8601(high_time, high_ts_str3, sizeof(high_ts_str3));
        printf("\t[INFO]: prune_collect-3: Keep msg[%i] timestamp[%s][%lld] disk_offset[%lld] \n",
               keep_sqn, high_ts_str3, (long long)high_time, (long long)msg_offset);
        dec->action     = PRUNE_ACTION_TAIL;
        dec->msg_offset = msg_offset;
        return 0;
    } else {
        /* Special case 4: wrapped cache — build per-message filter list */
        int low_sqn  = srp_repo->low_sqn;
        int high_sqn = srp_repo->high_sqn;
        int num_sqns = high_sqn - low_sqn + 1;
        free(repo_msg);
        printf("\t[INFO]: prune_collect-4: wrapped cache, collecting filter entries.\n");
        filter_msg_t *entries = malloc((size_t)num_sqns * sizeof(filter_msg_t));
        if (entries == NULL) {
            perror("\t[ERROR]:prune_collect-4() malloc entries failed");
            return -1;
        }
        char mbuf[LBM_SRP_DISK_MAX_CKSUM_MSG_LEN];
        lbm_srp_repo_msg_t *rmsg = malloc(sizeof(lbm_srp_repo_msg_t));
        if (rmsg == NULL) {
            perror("\t[ERROR]:prune_collect-4() malloc rmsg failed");
            free(entries);
            return -1;
        }
        rmsg->buff = mbuf;
        for (sqn = low_sqn; sqn <= high_sqn; ++sqn) {
            int idx = sqn - low_sqn;
            entries[idx].sqn = sqn;
            result = lbm_srp_get_repo_message(srp_repo, sqn, rmsg);
            if (result == 1) {
                entries[idx].disk_offset = rmsg->disk_offset;
                entries[idx].disk_len    = rmsg->disk_len;
                entries[idx].tsp_sec     = rmsg->tsp.tv_sec;
                entries[idx].keep = (rmsg->disk_len > 0 &&
                                     (rmsg->tsp.tv_sec >= threshold ||
                                      sqn == high_sqn)) ? 1 : 0;
                entries[idx].status      = 0;
            } else if (result == 0) {
                entries[idx].status      = 1; /* lost */
                entries[idx].keep        = 0;
            } else {
                entries[idx].status      = -1;
                entries[idx].keep        = 0;
                entries[idx].repo_status = srp_repo->repo_status;
                entries[idx].error_info  = srp_repo->error_info;
            }
        }
        free(rmsg);
        dec->action      = PRUNE_ACTION_FILTER;
        dec->low_sqn     = low_sqn;
        dec->high_sqn    = high_sqn;
        dec->end_offset  = srp_repo->end_offset;
        dec->entries     = entries;
        dec->num_entries = num_sqns;
        return 0;
    }
}

/*
 * prune_execute - Phase 2: perform the file I/O decided by prune_collect().
 *
 * Called AFTER lbm_srp_delete() has released all LBM file handles, so the
 * cache file can be opened without a Windows sharing violation.
 *
 * Returns 0 on success, -1 on error.
 */
static int prune_execute(const prune_decision_t *dec)
{
    if (dec->action == PRUNE_ACTION_NONE)
        return 0;

    if (dec->action == PRUNE_ACTION_TAIL) {
        if (tailOffset(dec->cache_filename, dec->bkup_dir,
                       dec->msg_offset, dec->end_of_record) == 0) {
            printf("\t[INFO]:[%s] prune_execute: file pruned successfully (tail)\n",
                   dec->cache_filename);
            return 0;
        } else {
            fprintf(stderr, "\t[ERROR]: [%s] prune_execute: tailOffset failed\n",
                    dec->cache_filename);
            return -1;
        }
    }

    /* PRUNE_ACTION_FILTER: do the file I/O using the pre-collected entry list */
    const char *CacheFilename = last_path_sep(dec->cache_filename);
    if (CacheFilename == NULL) {
        CacheFilename = dec->cache_filename;
    } else {
        CacheFilename++;
    }

    char oldCacheFile[SR_FILENAME_MAXSIZE];
    char tmpCacheFile[SR_FILENAME_MAXSIZE];
    if (snprintf(oldCacheFile, sizeof(oldCacheFile), "%s/%s", dec->bkup_dir, CacheFilename) >= (int)sizeof(oldCacheFile)) {
        fprintf(stderr, "\t[ERROR]:prune_execute() new file path is too long.\n");
        return -1;
    }
    if (snprintf(tmpCacheFile, sizeof(tmpCacheFile), "%s%s", "tmp_", CacheFilename) >= (int)sizeof(tmpCacheFile)) {
        fprintf(stderr, "\t[ERROR]:prune_execute() tmp file path is too long.\n");
        return -1;
    }

    if (moveCacheFileIdx(dec->cache_filename, oldCacheFile) == -1) {
        fprintf(stderr, "\t[WARNING]:prune_execute() Could not move cache Index file\n");
    }

    printf("\t[INFO]:prune_execute(): Copying %s to %s \n", dec->cache_filename, oldCacheFile);
    FILE *fsrc = fopen(dec->cache_filename, "rb+");
    if (!fsrc) { perror("\t[ERROR]:prune_execute() fopen cacheFile"); return -1; }
    FILE *fdst = fopen(oldCacheFile, "wb");
    if (!fdst) { perror("\t[ERROR]:prune_execute() fopen dest"); fclose(fsrc); return -1; }
    FILE *ftmp = fopen(tmpCacheFile, "wb");
    if (!ftmp) { perror("\t[ERROR]:prune_execute() fopen tmp"); fclose(fsrc); fclose(fdst); return -1; }

    char buf[8192];
    size_t n, nwritten;
    off_t bytes_copied = 0;

    /* Copy entire source to backup */
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        if (fwrite(buf, 1, n, fdst) != n) {
            perror("\t[ERROR]:prune_execute() fwrite backup");
            fclose(fsrc); fclose(fdst); fclose(ftmp);
            return -1;
        }
    }
    fclose(fdst);
    if (terse_print_flag == 0)
        printf("\t[INFO]:prune_execute() backup complete (low SQN:%i) (high SQN:%i)\n",
               dec->low_sqn, dec->high_sqn);

    if (fseeko(fsrc, 0, SEEK_SET) != 0) {
        perror("\t[ERROR]:prune_execute() fseeko reset");
        fclose(fsrc); fclose(ftmp); return -1;
    }

    /* Write kept messages to temp file */
    int i;
    for (i = 0; i < dec->num_entries; i++) {
        int sqn = dec->entries[i].sqn;
        if (dec->entries[i].status == 1) {
            printf("\t[WARNING]:prune_execute() Message sqn: %u was unrecoverably lost.\n", sqn);
            continue;
        }
        if (dec->entries[i].status < 0) {
            fprintf(stderr, "\t[ERROR]:Message sqn [%u] read failed; error code [%d] error info [%d]\n",
                    sqn, dec->entries[i].repo_status, dec->entries[i].error_info);
            continue;
        }
        if (dec->entries[i].disk_len == 0) {
            printf("\t[WARNING]:prune_execute() 0 bytes for msg sqn %i\n", sqn);
            continue;
        }
        if (!dec->entries[i].keep) {
            if (terse_print_flag == 0)
                printf("\t[DEBUG]:prune_execute() Pruning MSG: msg(%i) tsp=%lld\n",
                       sqn, (long long)dec->entries[i].tsp_sec);
            continue;
        }
        if (fseeko(fsrc, dec->entries[i].disk_offset, SEEK_SET) != 0) {
            perror("\t[ERROR]:prune_execute() fseeko msg");
            fclose(fsrc); fclose(ftmp); return -1;
        }
        if (terse_print_flag == 0)
            printf("\t[DEBUG]:prune_execute() msg(%i) seek to=%lld\n",
                   sqn, (long long)dec->entries[i].disk_offset);
        off_t num_bytes = dec->entries[i].disk_len + UMEDISK_REC_HDR_SZ;
        if ((n = fread(buf, 1, num_bytes, fsrc)) == num_bytes) {
            nwritten = fwrite(buf, 1, num_bytes, ftmp);
            if (nwritten != num_bytes) {
                perror("\t[ERROR]:prune_execute() fwrite msg");
                fclose(fsrc); fclose(ftmp); return 1;
            }
            bytes_copied += (off_t)nwritten;
            if (terse_print_flag == 0)
                printf("\t[DEBUG]:sqn[%i] written: %lld\n", sqn, (long long)nwritten);
        } else {
            fprintf(stderr, "\t[ERROR]:prune_execute() read msg %i failed num_bytes:%lld\n",
                    sqn, (long long)num_bytes);
            fclose(fsrc); fclose(ftmp); return 1;
        }
    }

    /* Append end of record marker */
    if (fseeko(fsrc, dec->end_offset, SEEK_SET) != 0) {
        perror("\t[ERROR]:prune_execute() fseeko end marker");
        fclose(fsrc); fclose(ftmp); return 1;
    }
    if ((n = fread(buf, 1, UMEDISK_REC_MARKER_SZ, fsrc)) == UMEDISK_REC_MARKER_SZ) {
        umedisk_rec_marker_t *rec_marker_pos = (umedisk_rec_marker_t *)(buf);
        if (terse_print_flag == 0)
            printf("\t[DEBUG]:prune_execute() end marker start_offset=%lld -> 0\n",
                   (long long)rec_marker_pos->start_offset);
        rec_marker_pos->start_offset = 0;
        size_t nw2 = fwrite(buf, 1, UMEDISK_REC_MARKER_SZ, ftmp);
        if (nw2 != UMEDISK_REC_MARKER_SZ) {
            perror("\t[ERROR]:prune_execute() fwrite end marker");
            fclose(fsrc); fclose(ftmp); return 1;
        }
        bytes_copied += (off_t)nw2;
    } else {
        fprintf(stderr, "\t[ERROR]:prune_execute() END marker read error\n");
        fclose(fsrc); fclose(ftmp); return 1;
    }
    fclose(fsrc);
    fclose(ftmp);

    int ret = replace_file(dec->cache_filename, tmpCacheFile);
    if (ret != 0) {
        fprintf(stderr, "\t[ERROR]:prune_execute() Could not replace %s with %s\n",
                dec->cache_filename, tmpCacheFile);
        return -1;
    }

    off_t filesize_before = get_file_size(oldCacheFile);
    printf("\t[INFO]:prune_execute() File size before: %lld bytes\n", (long long)filesize_before);
    off_t filesize_after = get_file_size(dec->cache_filename);
    printf("\t[INFO]:prune_execute() File size after:  %lld bytes\n", (long long)filesize_after);

    return 0;
}



int tailOffset(const char *cacheFile, const char *old_dir, off_t prune_offset, off_t end_record) {
    char oldCacheFile[SR_FILENAME_MAXSIZE]; 
    const char *CacheFilename = last_path_sep(cacheFile); // Find the last path separator

    if (CacheFilename == NULL) {
        CacheFilename = cacheFile; // No '/', assume it's just the filename
    } else {
        CacheFilename++; // Move past the '/'
    }

    // Construct the file path for the unmodified cache file
    if (snprintf(oldCacheFile, sizeof(oldCacheFile), "%s/%s", old_dir, CacheFilename) >= sizeof(oldCacheFile)) {
        fprintf(stderr, "\t[ERROR]: tailOffset() New file path is too long.\n");
        return -1;
    }

    printf("\t[INFO]: tailOffset() Copying %s to %s : prune_offset[%lld] end_record[%lld] \n", cacheFile, oldCacheFile, (long long)prune_offset, (long long)end_record);
    FILE *fsrc = fopen(cacheFile, "rb");
    if (!fsrc) {
        perror("\t[ERROR]: tailOffset() fopen cacheFile");
        return -1;
    }
    FILE *fdst = fopen(oldCacheFile, "wb");
    if (!fdst) {
        perror("\t[ERROR]: tailOffset() fopen dest");
        fclose(fsrc);
        return -1;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        if (fwrite(buf, 1, n, fdst) != n) {
            perror("\t[ERROR]: tailOffset() fwrite");
            fclose(fsrc);
            fclose(fdst);
            return -1;
        }
    }
    fclose(fsrc);
    fclose(fdst);

    if( moveCacheFileIdx(cacheFile, oldCacheFile) == -1){
	    fprintf(stderr, "\t[WARNING]: tailOffset() Could not move cache Index file ");
    }
    
    if(terse_print_flag == 0) printf("\t[INFO]: tailOffset() Original file copied now pruning ...  \n");

    off_t filesize_before = get_file_size(oldCacheFile);
    if(terse_print_flag == 0) printf("\t[INFO]: tailOffset() File size before pruning: %lld bytes\n", (long long)filesize_before);

    if(terse_print_flag == 0) printf("\t[INFO]: tailOffset() Pruning to %lld bytes after header...\n", (long long)prune_offset);
    if (prune_file_inplace(cacheFile, prune_offset, end_record) != 0) {
        fprintf(stderr, "\t[ERROR]:tailOffset() prune_file_inplace failed\n");
        return -1;
    }
    off_t filesize_after = get_file_size(cacheFile);
    if(terse_print_flag == 0) printf("\t[INFO]: tailOffset() File size after pruning: %lld bytes\n", (long long)filesize_after);

    return 0;
}




/**
 * Moves a file from its old path to a new directory, keeping the original filename.
 * @return 0 on success, -1 on failure.
 */
int moveFilesToDirectory(const char *oldStateFile, const char *oldCacheFile, const char *newDirectory) {
    char newStateFile[SR_FILENAME_MAXSIZE]; 
    char newCacheFile[SR_FILENAME_MAXSIZE]; 

    // Extract the filename from the old path
    const char *StateFilename = last_path_sep(oldStateFile); // Find the last path separator
    if (StateFilename == NULL) {
        StateFilename = oldStateFile; // No '/', assume it's just the filename
    } else {
        StateFilename++; // Move past the '/'
    }

    const char *CacheFilename = last_path_sep(oldCacheFile); // Find the last path separator
    if (CacheFilename == NULL) {
        CacheFilename = oldCacheFile; // No '/', assume it's just the filename
    } else {
        CacheFilename++; // Move past the '/'
    }

    // Construct the new state file path
    if (snprintf(newStateFile, sizeof(newStateFile), "%s/%s", newDirectory, StateFilename) >= sizeof(newStateFile)) {
        fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() new state file path is too long.\n");
        return -1;
    }

    // Construct the new cache file path
    if (snprintf(newCacheFile, sizeof(newCacheFile), "%s/%s", newDirectory, CacheFilename) >= sizeof(newCacheFile)) {
        fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() new cache file path is too long.\n");
        return -1;
    }


    // Attempt to rename/move the file
    if (rename(oldStateFile, newStateFile) == 0) {
        printf("\t[INFO] moveFilesToDirectory():File '%s' renamed and moved to '%s' successfully.\n", oldStateFile, newStateFile);
    } else {
        // Handle rename errors
        switch (errno) {
            case ENOENT:
                fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() Either source file '%s' or target directory '%s' does not exist.\n", oldStateFile, newDirectory);
                break;
            case EACCES:
                fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() Permission denied for moving file to '%s'.\n", newDirectory);
                break;
            case EXDEV:
                fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() Cannot move file across different filesystems. Consider a different directory or using -P option.\n");
                break;
            default:
                perror("\t[ERROR]:moveFilesToDirectory() renaming/moving file");
                break;
        }
        return -1; // Failure
    }

    if (rename(oldCacheFile, newCacheFile) == 0) {
        printf("\t[INFO] moveFilesToDirectory():File '%s' renamed and moved to '%s' successfully.\n", oldCacheFile, newCacheFile);
    } else {
        // Handle rename errors
        switch (errno) {
            case ENOENT:
                fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() Either source file '%s' or target directory '%s' does not exist.\n", oldCacheFile, newDirectory);
                break;
            case EACCES:
                fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() Permission denied for moving file to '%s'.\n", newDirectory);
                break;
            case EXDEV:
                fprintf(stderr, "\t[ERROR]:moveFilesToDirectory() Cannot move file across different filesystems. Consider a different directory or using -P option.\n");
                break;
            default:
                perror("\t[ERROR]:moveFilesToDirectory() renaming/moving file");
                break;
        }
        return -1; // Failure
    }

    if( moveCacheFileIdx(oldCacheFile, newCacheFile) == -1){
	    fprintf(stderr, "\t[WARNING] moveFilesToDirectory(): Could not move cache Index file ");
    }

    return 0;
}

static void move_repo_timestamp(lbm_srp_repo_t *srp_repo, time_t move_timestamp_threshold, char * old_dir) {
	char low_ts_str[32];
	char high_ts_str[32];
	time_t low_time, high_time;
    	int i,result;
	char msg_buffer[LBM_SRP_DISK_MAX_CKSUM_MSG_LEN];
	

	lbm_srp_repo_msg_t *repo_msg = malloc(sizeof(lbm_srp_repo_msg_t));
	if (repo_msg == NULL) {
    	     perror("malloc failed");
             return; 
	}
	repo_msg->buff = msg_buffer;

	if (srp_repo->repo_status != LBM_SRP_REPO_VALID) {
		fprintf(stderr, "\t[ERROR]:move_repo_timestamp() for %s and %s:",srp_repo->state_filename, srp_repo->cache_filename);
		fprintf(stderr, "invalid repo status. error_info:0x%08x\n", srp_repo->error_info);
		free(repo_msg);
		return;
	}
	
	printf("[INFO]:move_repo_timestamp() for %s and %s:",srp_repo->state_filename, srp_repo->cache_filename);
	if( srp_repo->num_msgs == 0 ){
		printf("No messages in repo\n");
		free(repo_msg);
		if (moveFilesToDirectory(srp_repo->state_filename, srp_repo->cache_filename, old_dir) == 0) {
			printf("\t[INFO]:move_repo_timestamp() file move completed successfully \n");
		} else {
			fprintf(stderr, "\t[ERROR]:move_repo_timestamp() File move operation failed \n");
		}
		return;
	}

	result = lbm_srp_get_repo_message(srp_repo, srp_repo->low_sqn, repo_msg);
	if (result != 1){ 
		/* trouble reading repo */
		fprintf(stderr, "\t[ERROR]:move_repo_timestamp(): result of lbm_srp_get_repo_message (%i) = %i \n", srp_repo->low_sqn, result);
		return;
	}
	low_time = repo_msg->tsp.tv_sec;
	epoch_to_iso8601(low_time, low_ts_str, sizeof(low_ts_str));
	if(terse_print_flag == 0) printf("\n\t[DEBUG]:Lowest msg[%u] timestamp: %s\n", srp_repo->low_sqn, low_ts_str);

	result = lbm_srp_get_repo_message(srp_repo, srp_repo->high_sqn, repo_msg);
	if (result != 1){ 
		/* trouble reading repo */
		fprintf(stderr, "\t[ERROR]:move_repo_timestamp(): result of lbm_srp_get_repo_message (%i) = %i \n", srp_repo->high_sqn, result);
		return;
	}
	high_time = repo_msg->tsp.tv_sec;
	epoch_to_iso8601(high_time, high_ts_str, sizeof(high_ts_str));
	if(terse_print_flag == 0) printf("\t[DEBUG]:Highest msg[%u] timestamp: %s\n", srp_repo->high_sqn, high_ts_str);

	if (move_timestamp_threshold > high_time) {
		//printf("\t[INFO]:move_repo_timestamp() highest message outside threshold range: moving %s and %s to %s\n",srp_repo->state_filename, srp_repo->cache_filename, old_dir);
		printf("\n");
		if (moveFilesToDirectory(srp_repo->state_filename, srp_repo->cache_filename, old_dir) == 0) {
			printf("\t[INFO]move_repo_timestamp(): file move completed successfully \n");
		} else {
			fprintf(stderr, "\t[ERROR]:move_repo_timestamp(): File move operation failed \n");
		}
	} else {
		printf("\t[INFO]:move_repo_timestamp(): Threshold within range: skipping \n");
	}
	free(repo_msg);
}/*move_repo_timestamp() */

int main(int argc, char **argv)
{
	struct Options *opts = &profile_repo_options;
	const lbm_srp_t *srp_handle;
	lbm_srp_repo_t *srp_repo;
	int rc, repo_idx, num_repos = 0, overall_rc = 0;
	prune_decision_t *prune_decisions = NULL;
	char *cache_file_search_path;

	process_cmdline(argc, argv, opts);

	cache_file_search_path = (opts->skip_cache) ? NULL : opts->cache_file_search_path;
	if ((num_repos = lbm_srp_create(opts->state_file_search_path, cache_file_search_path, opts->check_checksum, &srp_handle)) < 0) {
		exit(1);
	}

	if (opts->prune_repo_flag == 1) {
		prune_decisions = calloc((size_t)num_repos, sizeof(prune_decision_t));
		if (prune_decisions == NULL) { perror("[ERROR]: calloc prune_decisions"); exit(1); }
	}

	for (repo_idx = 0; repo_idx < num_repos; ++repo_idx) {
		printf("[INFO]: Examining repository at index: %d\n", repo_idx);
		rc = lbm_srp_get_repo_state(&srp_handle, repo_idx, &srp_repo);
		switch (rc) {
		case 1:
			if(opts->prune_repo_flag == 1 ){
				/* Collect pruning decision from LBM — no file I/O yet */
				if (prune_collect(srp_repo, opts->prune_timestamp_threshold,
				                  opts->old_directory, &prune_decisions[repo_idx]) < 0) {
					fprintf(stderr, "[ERROR]: prune_collect failed for repo %d\n", repo_idx);
					overall_rc = 1;
				}
			} else if(opts->move_repo_flag == 1 ){
				move_repo_timestamp(srp_repo, opts->move_timestamp_threshold, opts->old_directory);
			} else {
				print_repo_state(srp_repo, opts->parse, opts->skip_cache, opts->truncate_length, opts->terse, opts->lastmsgonly_flag);
			}
			if (lbm_srp_free_repo_state(srp_repo) < 0) {
				exit(1);
			}
			break;
		case 0:
			printf("[INFO]: ***repository skipped\n\n");
			break;
		default:	/* error condition */
			exit(1);
		}
	}

	/* Release ALL file handles held by the LBM session */
	if (lbm_srp_delete(&srp_handle) < 0) {
		exit(1);
	}

	/* Phase 2: perform file I/O now that no LBM handles are open */
	if (prune_decisions != NULL) {
		for (repo_idx = 0; repo_idx < num_repos; ++repo_idx) {
			if (prune_execute(&prune_decisions[repo_idx]) < 0) {
				fprintf(stderr, "[ERROR]: prune_execute failed for repo %d\n", repo_idx);
				overall_rc = 1;
			}
			free(prune_decisions[repo_idx].entries);
		}
		free(prune_decisions);
	}

	exit(overall_rc);
}

