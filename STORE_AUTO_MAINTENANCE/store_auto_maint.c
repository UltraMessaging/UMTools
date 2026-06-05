/*
  (C) Copyright 2025,2026 Informatica Inc.  Permission is granted to licensees to use
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

/*
 * store_auto_maint.c - Automated UME Store maintenance wrapper.
 *
 * Runs umestored as a child process, monitors its output for error keywords,
 * and periodically executes the maintenance procedure (stop, prune, verify,
 * restart) on a configurable schedule.
 *
 * If restart after maintenance fails, implements 3-tier recovery:
 *   1. Restart with pruned files
 *   2. Revert to backup files and restart
 *   3. Wipe state/cache directories and restart fresh
 */

/* Suppress MSVC deprecation warnings for POSIX functions (strncpy, strtok,
 * gmtime, etc.).  These functions are used correctly and the MSVC _s variants
 * would break POSIX portability.  Must be defined before any CRT #include. */
#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>

#ifdef _WIN32
	#include <windows.h>
	#include <process.h>
	#include <io.h>
	#define PATH_SEP '\\'
	#define PATH_SEP_STR "\\"
	#define SCRIPT_EXT ".bat"
	#define pid_t DWORD
	#define sleep(s) Sleep((s) * 1000)
	#define snprintf _snprintf
	#define strncasecmp _strnicmp
	#define strcasecmp  _stricmp
	#define popen       _popen
	#define pclose      _pclose

	/* Minimal POSIX getopt() — not provided by MSVC */
	static char  *optarg = NULL;
	static int    optind = 1;
	static int    opterr = 1;
	static int    optopt = '?';
	static int getopt(int argc, char * const argv[], const char *optstring)
	{
		static int sp = 1;
		const char *cp;
		int c;
		if (sp == 1) {
			if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
				return -1;
			if (strcmp(argv[optind], "--") == 0) { optind++; return -1; }
		}
		optopt = c = (unsigned char)argv[optind][sp];
		cp = strchr(optstring, c);
		if (!cp || c == ':') {
			if (opterr) fprintf(stderr, "Unknown option: -%c\n", c);
			if (argv[optind][++sp] == '\0') { optind++; sp = 1; }
			return '?';
		}
		if (cp[1] == ':') {
			if (argv[optind][sp + 1] != '\0') {
				optarg = &argv[optind++][sp + 1];
			} else if (++optind >= argc) {
				if (opterr) fprintf(stderr, "Option -%c requires an argument\n", c);
				sp = 1; optopt = c;
				return optstring[0] == ':' ? ':' : '?';
			} else {
				optarg = argv[optind++];
			}
			sp = 1;
		} else {
			optarg = NULL;
			if (argv[optind][++sp] == '\0') { optind++; sp = 1; }
		}
		return c;
	}
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <sys/stat.h>
	#include <errno.h>
	#include <fcntl.h>
	#include <dirent.h>
	#include <libgen.h>
	#define PATH_SEP '/'
	#define PATH_SEP_STR "/"
	#define SCRIPT_EXT ".sh"
#endif

#include "xml_config_parser.h"

/* ================================================================
 * Constants and defaults
 * ================================================================ */

#define PROG_NAME "store_auto_maint"
#define VERSION "1.0.0"

#define DEFAULT_SCHEDULE "weekly:fri:00:00"
#define DEFAULT_KEYWORD "crit"
#define DEFAULT_KEYWORD_THRESHOLD 2
#define DEFAULT_KEYWORD_WINDOW 60
#define DEFAULT_BACKUP_DIR "./UMDIR"
#define DEFAULT_MAINT_LOG "./store_maint.log"
#define DEFAULT_UMESTORED_PATH "umestored"
#define DEFAULT_UMESNAPREPO_PATH "umesnaprepo"
#define DEFAULT_MAINT_SCRIPT_DIR "."
#define DEFAULT_PID_FILE "./umestored_managed.pid"

#define PIPE_BUF_SIZE 4096
#define LOG_LINE_MAX 2048
#define MAX_KEYWORD_EVENTS 1024
#define CHILD_STARTUP_WAIT_SECS 5
#define CHILD_RESTART_MAX_ATTEMPTS 3

#define DEFAULT_SHUTDOWN_WAIT 3600    /* 1 hour: first SIGINT grace period */
#define SHUTDOWN_ESCALATE_WAIT 600    /* 10 minutes: double-SIGINT grace period */

/* ================================================================
 * Data structures
 * ================================================================ */

/* Keyword severity levels (ordered) */
typedef enum {
	SEV_WARN = 0,
	SEV_ERR,
	SEV_ALERT,
	SEV_CRIT,
	SEV_COUNT
} severity_t;

static const char *severity_names[] = {"warn", "err", "alert", "crit"};

/* Circular buffer for keyword event timestamps */
typedef struct {
	time_t timestamps[MAX_KEYWORD_EVENTS];
	int head;
	int count;
} keyword_ring_t;

/* Maintenance statistics */
typedef struct {
	int initiated;
	int completed;
	int errors;     /* maintenance script failed but store restarted */
	int failed;     /* all restart attempts failed */
} maint_stats_t;

/* Schedule types */
typedef enum {
	SCHED_WEEKLY,     /* weekly:DAY:HH:MM */
	SCHED_DAILY,      /* daily:HH:MM */
	SCHED_INTERVAL    /* interval:SECONDS */
} schedule_type_t;

typedef struct {
	schedule_type_t type;
	int day_of_week;   /* 0=Sun, 1=Mon, ..., 5=Fri, 6=Sat (for SCHED_WEEKLY) */
	int hour;
	int minute;
	int interval_secs; /* for SCHED_INTERVAL */
} schedule_t;

/* Main program options */
typedef struct {
	char xml_config_path[MAX_PATH_LEN];
	char schedule_str[128];
	char keyword_str[64];
	int keyword_threshold;    /* N occurrences */
	int keyword_window;       /* M seconds */
	char backup_dir[MAX_PATH_LEN];
	char maint_log_path[MAX_PATH_LEN];
	char umestored_path[MAX_PATH_LEN];
	char umesnaprepo_path[MAX_PATH_LEN];
	char maint_script_dir[MAX_PATH_LEN];
	char store_filter[MAX_STORE_NAME]; /* -S: process only this store */
	char pid_file_path[MAX_PATH_LEN]; /* -P: PID file for managed umestored */
	int shutdown_wait;        /* -W: seconds to wait for graceful shutdown */
	int utc_timestamps;       /* -u: log timestamps in UTC instead of local time */
	int auto_confirm;         /* -y: skip confirmation */
	int validate_only;        /* -V: validate config and exit */
	/* Passthrough args for umestored (everything after -- on the command line) */
	char **umestored_extra_argv;
	int umestored_extra_argc;
} options_t;

/* Global state */
static options_t g_opts;
static ume_config_t g_config;
static schedule_t g_schedule;
static severity_t g_min_severity;
static keyword_ring_t g_keyword_ring;
static maint_stats_t g_stats;
static FILE *g_log_fp = NULL;
static volatile int g_running = 1;
static volatile int g_child_alive = 0;
static pid_t g_child_pid = 0;
static time_t g_last_maint_time = 0;
/* Two SIGINTs within DOUBLE_INT_WINDOW seconds skip phase-1 wait */
static volatile sig_atomic_t g_fast_shutdown = 0;
static volatile time_t g_first_int_time = 0;
#define DOUBLE_INT_WINDOW 5

#ifndef _WIN32
static int g_pipe_fd[2] = {-1, -1};
#else
static HANDLE g_pipe_read = INVALID_HANDLE_VALUE;
static HANDLE g_pipe_write = INVALID_HANDLE_VALUE;
static HANDLE g_child_handle = INVALID_HANDLE_VALUE;
#endif

/* ================================================================
 * Logging
 * ================================================================ */

static void maint_log(const char *level, const char *fmt, ...)
{
	va_list ap;
	char timestamp[64];
	char msg[LOG_LINE_MAX];
	time_t now = time(NULL);
	struct tm *tm_info;

	tm_info = g_opts.utc_timestamps ? gmtime(&now) : localtime(&now);
	strftime(timestamp, sizeof(timestamp),
	         g_opts.utc_timestamps ? "%Y-%m-%d %H:%M:%S UTC" : "%Y-%m-%d %H:%M:%S",
	         tm_info);

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	fprintf(stdout, "[%s] [%s] %s\n", timestamp, level, msg);
	fflush(stdout);

	if (g_log_fp) {
		fprintf(g_log_fp, "[%s] [%s] %s\n", timestamp, level, msg);
		fflush(g_log_fp);
	}
}

#define LOG_INFO(...)  maint_log("INFO",  __VA_ARGS__)
#define LOG_WARN(...)  maint_log("WARN",  __VA_ARGS__)
#define LOG_ERROR(...) maint_log("ERROR", __VA_ARGS__)
#define LOG_STATS(...) maint_log("STATS", __VA_ARGS__)

/* ================================================================
 * Signal handling
 * ================================================================ */

#ifndef _WIN32
static void sig_handler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM) {
		time_t now = time(NULL);
		if (g_first_int_time != 0 && (now - g_first_int_time) <= DOUBLE_INT_WINDOW) {
			g_fast_shutdown = 1;
			LOG_WARN("Second signal within %ds; skipping phase-1 wait.", DOUBLE_INT_WINDOW);
		} else {
			g_first_int_time = now;
			LOG_INFO("Received signal %d, initiating shutdown...", sig);
		}
		g_running = 0;
	}
	if (sig == SIGCHLD) {
		g_child_alive = 0;
	}
}
#else
static BOOL WINAPI console_handler(DWORD type)
{
	if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
		time_t now = time(NULL);
		if (g_first_int_time != 0 && (now - g_first_int_time) <= DOUBLE_INT_WINDOW) {
			g_fast_shutdown = 1;
			LOG_WARN("Second console event within %ds; skipping phase-1 wait.", DOUBLE_INT_WINDOW);
		} else {
			g_first_int_time = now;
			LOG_INFO("Received console event, initiating shutdown...");
		}
		g_running = 0;
		return TRUE;
	}
	return FALSE;
}
#endif

static void setup_signals(void)
{
#ifndef _WIN32
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);
#else
	SetConsoleCtrlHandler(console_handler, TRUE);
#endif
}

/* ================================================================
 * Option parsing
 * ================================================================ */

static const char *usage_text =
	"Usage: " PROG_NAME " -x CONFIG_FILE [options]\n"
	"\n"
	"Required:\n"
	"  -x FILE   UME store XML configuration file\n"
	"\n"
	"Options:\n"
	"  -f SCHED  Maintenance schedule (default: " DEFAULT_SCHEDULE ")\n"
	"            Formats: weekly:DAY:HH:MM  daily:HH:MM  interval:SECONDS\n"
	"            DAY: sun,mon,tue,wed,thu,fri,sat\n"
	"  -k LEVEL  Minimum keyword severity to monitor (default: " DEFAULT_KEYWORD ")\n"
	"            Levels (lowest to highest): warn, err, alert, crit\n"
	"            Matches the specified level and all higher levels.\n"
	"            When threshold is exceeded the process exits.\n"
	"  -N NUM    Keyword occurrence threshold (default: 2)\n"
	"  -M SECS   Keyword time window in seconds (default: 60)\n"
	"  -d DIR    Backup directory for old files (default: " DEFAULT_BACKUP_DIR ")\n"
	"  -L FILE   Maintenance log file (default: " DEFAULT_MAINT_LOG ")\n"
	"  -e PATH   Path to umestored binary (default: umestored)\n"
	"  -r PATH   Path to umesnaprepo binary (default: umesnaprepo)\n"
	"  -R DIR    Directory containing maintenance script (default: .)\n"
	"  -S NAME   Process only this store (default: all stores)\n"
	"  -W SECS   Graceful shutdown wait in seconds (default: 3600)\n"
	"  -P FILE   PID file for managed umestored (default: " DEFAULT_PID_FILE ")\n"
	"            Checked at startup to detect stale or already-running instances.\n"
	"  -u        Log timestamps in UTC (default: local time)\n"
	"  -y        Skip confirmation prompt\n"
	"  -V        Validate configuration and exit\n"
	"  -h        Display this help\n"
	"\n"
	"Passthrough options for umestored (place after --):\n"
	"  -- [UMESTORED_OPTS]  Options passed directly to umestored\n"
	"  Example: store_auto_maint -x config.xml -- -u -a 1,3,5\n"
;

static void print_usage(void)
{
	fprintf(stderr, "%s v%s\n%s", PROG_NAME, VERSION, usage_text);
}

static int parse_day_of_week(const char *s)
{
	if (strncasecmp(s, "sun", 3) == 0) return 0;
	if (strncasecmp(s, "mon", 3) == 0) return 1;
	if (strncasecmp(s, "tue", 3) == 0) return 2;
	if (strncasecmp(s, "wed", 3) == 0) return 3;
	if (strncasecmp(s, "thu", 3) == 0) return 4;
	if (strncasecmp(s, "fri", 3) == 0) return 5;
	if (strncasecmp(s, "sat", 3) == 0) return 6;
	return -1;
}

static int parse_schedule(const char *str, schedule_t *sched)
{
	char buf[128];
	char *tok;

	memset(sched, 0, sizeof(*sched));
	strncpy(buf, str, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	tok = strtok(buf, ":");
	if (!tok) return -1;

	if (strcasecmp(tok, "weekly") == 0) {
		sched->type = SCHED_WEEKLY;
		tok = strtok(NULL, ":");
		if (!tok) return -1;
		sched->day_of_week = parse_day_of_week(tok);
		if (sched->day_of_week < 0) {
			fprintf(stderr, "ERROR: Invalid day of week: %s\n", tok);
			return -1;
		}
		tok = strtok(NULL, ":");
		if (!tok) return -1;
		sched->hour = atoi(tok);
		tok = strtok(NULL, ":");
		if (!tok) return -1;
		sched->minute = atoi(tok);
	} else if (strcasecmp(tok, "daily") == 0) {
		sched->type = SCHED_DAILY;
		tok = strtok(NULL, ":");
		if (!tok) return -1;
		sched->hour = atoi(tok);
		tok = strtok(NULL, ":");
		if (!tok) return -1;
		sched->minute = atoi(tok);
	} else if (strcasecmp(tok, "interval") == 0) {
		sched->type = SCHED_INTERVAL;
		tok = strtok(NULL, ":");
		if (!tok) return -1;
		sched->interval_secs = atoi(tok);
		if (sched->interval_secs <= 0) {
			fprintf(stderr, "ERROR: Invalid interval: %s\n", tok);
			return -1;
		}
	} else {
		fprintf(stderr, "ERROR: Unknown schedule type: %s\n", tok);
		return -1;
	}

	if (sched->type == SCHED_WEEKLY || sched->type == SCHED_DAILY) {
		if (sched->hour < 0 || sched->hour > 23) {
			fprintf(stderr, "ERROR: Invalid hour (must be 0-23): %d\n", sched->hour);
			return -1;
		}
		if (sched->minute < 0 || sched->minute > 59) {
			fprintf(stderr, "ERROR: Invalid minute (must be 0-59): %d\n", sched->minute);
			return -1;
		}
	}

	return 0;
}

static severity_t parse_severity(const char *str)
{
	int i;
	for (i = 0; i < SEV_COUNT; i++) {
		if (strcasecmp(str, severity_names[i]) == 0)
			return (severity_t)i;
	}
	return SEV_CRIT; /* default */
}

static int parse_options(int argc, char **argv)
{
	int opt;

	/* Set defaults */
	memset(&g_opts, 0, sizeof(g_opts));
	strncpy(g_opts.schedule_str, DEFAULT_SCHEDULE, sizeof(g_opts.schedule_str) - 1);
	strncpy(g_opts.keyword_str, DEFAULT_KEYWORD, sizeof(g_opts.keyword_str) - 1);
	g_opts.keyword_threshold = DEFAULT_KEYWORD_THRESHOLD;
	g_opts.keyword_window = DEFAULT_KEYWORD_WINDOW;
	strncpy(g_opts.backup_dir, DEFAULT_BACKUP_DIR, sizeof(g_opts.backup_dir) - 1);
	strncpy(g_opts.maint_log_path, DEFAULT_MAINT_LOG, sizeof(g_opts.maint_log_path) - 1);
	strncpy(g_opts.umestored_path, DEFAULT_UMESTORED_PATH, sizeof(g_opts.umestored_path) - 1);
	strncpy(g_opts.umesnaprepo_path, DEFAULT_UMESNAPREPO_PATH, sizeof(g_opts.umesnaprepo_path) - 1);
	strncpy(g_opts.maint_script_dir, DEFAULT_MAINT_SCRIPT_DIR, sizeof(g_opts.maint_script_dir) - 1);
	strncpy(g_opts.pid_file_path, DEFAULT_PID_FILE, sizeof(g_opts.pid_file_path) - 1);
	g_opts.shutdown_wait = DEFAULT_SHUTDOWN_WAIT;

	while ((opt = getopt(argc, argv, "x:f:k:N:M:d:L:e:r:R:S:W:P:uyVh")) != -1) {
		switch (opt) {
		case 'x':
			strncpy(g_opts.xml_config_path, optarg, MAX_PATH_LEN - 1);
			break;
		case 'f':
			strncpy(g_opts.schedule_str, optarg, sizeof(g_opts.schedule_str) - 1);
			break;
		case 'k':
			strncpy(g_opts.keyword_str, optarg, sizeof(g_opts.keyword_str) - 1);
			break;
		case 'N':
			g_opts.keyword_threshold = atoi(optarg);
			break;
		case 'M':
			g_opts.keyword_window = atoi(optarg);
			break;
		case 'd':
			strncpy(g_opts.backup_dir, optarg, MAX_PATH_LEN - 1);
			break;
		case 'L':
			strncpy(g_opts.maint_log_path, optarg, MAX_PATH_LEN - 1);
			break;
		case 'e':
			strncpy(g_opts.umestored_path, optarg, MAX_PATH_LEN - 1);
			break;
		case 'r':
			strncpy(g_opts.umesnaprepo_path, optarg, MAX_PATH_LEN - 1);
			break;
		case 'R':
			strncpy(g_opts.maint_script_dir, optarg, MAX_PATH_LEN - 1);
			break;
		case 'S':
			strncpy(g_opts.store_filter, optarg, MAX_STORE_NAME - 1);
			break;
		case 'W':
			g_opts.shutdown_wait = atoi(optarg);
			if (g_opts.shutdown_wait <= 0) {
				fprintf(stderr, "ERROR: Invalid shutdown wait: %s\n", optarg);
				return -1;
			}
			break;
		case 'P':
			strncpy(g_opts.pid_file_path, optarg, MAX_PATH_LEN - 1);
			break;
		case 'u':
			g_opts.utc_timestamps = 1;
			break;
		case 'y':
			g_opts.auto_confirm = 1;
			break;
		case 'V':
			g_opts.validate_only = 1;
			break;
		case 'h':
			print_usage();
			exit(0);
		default:
			print_usage();
			return -1;
		}
	}

	if (g_opts.xml_config_path[0] == '\0') {
		fprintf(stderr, "ERROR: XML config file (-x) is required\n\n");
		print_usage();
		return -1;
	}

	/* Capture any remaining args after getopt finishes (i.e. after --) as umestored passthrough args */
	if (optind < argc) {
		g_opts.umestored_extra_argv = &argv[optind];
		g_opts.umestored_extra_argc = argc - optind;
	}

	return 0;
}

/* ================================================================
 * Binary validation
 * ================================================================ */

/*
 * Run "umesnaprepo -h" and verify the output contains the options
 * required for maintenance: -d (old-dir), -m (move-old), -P (prune), -l (lastmsgonly).
 * Returns 0 if all required options are present, -1 otherwise.
 */
static int validate_umesnaprepo(void)
{
	char cmd[MAX_PATH_LEN + 32];
	FILE *fp;
	char line[1024];
	int found_d = 0, found_m = 0, found_P = 0, found_l = 0;
	int got_output = 0;
	int rc;

	snprintf(cmd, sizeof(cmd), "\"%s\" -h 2>&1", g_opts.umesnaprepo_path);
	fp = popen(cmd, "r");
	if (!fp) {
		fprintf(stderr, "ERROR: Cannot execute '%s': %s\n",
		        g_opts.umesnaprepo_path, strerror(errno));
		fprintf(stderr, "  Ensure umesnaprepo is installed and accessible via PATH or use -r to specify its location.\n");
		return -1;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		got_output = 1;
		if (strstr(line, "--old-dir") || strstr(line, "-d,"))
			found_d = 1;
		if (strstr(line, "--move-old") || strstr(line, "-m,"))
			found_m = 1;
		if (strstr(line, "--prune") || strstr(line, "-P,"))
			found_P = 1;
		if (strstr(line, "--lastmsgonly") || strstr(line, "-l,"))
			found_l = 1;
		/* Check for shared library errors (binary exists but can't load) */
		if (strstr(line, "error while loading") || strstr(line, "cannot open shared object")) {
			fprintf(stderr, "WARNING: '%s' cannot load shared libraries: %s",
			        g_opts.umesnaprepo_path, line);
			fprintf(stderr, "  Set LD_LIBRARY_PATH or use -r to specify a statically-linked binary.\n");
			fprintf(stderr, "  Skipping umesnaprepo option validation.\n");
			pclose(fp);
			return 0; /* allow startup; will fail at maintenance time with a clear error */
		}
		/* Check for binary not found (command not found / no such file) */
		if (strstr(line, "command not found") || strstr(line, "No such file or directory")) {
			fprintf(stderr, "ERROR: '%s' not found: %s",
			        g_opts.umesnaprepo_path, line);
			fprintf(stderr, "  Ensure umesnaprepo is installed or use -r to specify its path.\n");
			pclose(fp);
			return -1;
		}
	}
	rc = pclose(fp);

	/* Binary not found at all */
	if (!got_output) {
#ifndef _WIN32
		if (WIFEXITED(rc) && WEXITSTATUS(rc) == 127) {
			fprintf(stderr, "ERROR: '%s' not found. Ensure it is installed or use -r to specify its path.\n",
			        g_opts.umesnaprepo_path);
			return -1;
		}
#endif
		fprintf(stderr, "WARNING: '%s' produced no output. Skipping option validation.\n",
		        g_opts.umesnaprepo_path);
		return 0;
	}

	if (!found_d || !found_m || !found_P || !found_l) {
		fprintf(stderr, "ERROR: '%s' does not support required maintenance options:\n",
		        g_opts.umesnaprepo_path);
		if (!found_d) fprintf(stderr, "  MISSING: -d, --old-dir       (move old files to directory)\n");
		if (!found_m) fprintf(stderr, "  MISSING: -m, --move-old      (move files older than timestamp)\n");
		if (!found_P) fprintf(stderr, "  MISSING: -P, --prune         (prune messages older than timestamp)\n");
		if (!found_l) fprintf(stderr, "  MISSING: -l, --lastmsgonly   (dump only last message in cache)\n");
		fprintf(stderr, "  The maintenance procedure requires an updated umesnaprepo.\n");
		fprintf(stderr, "  Use -r to specify the path to the correct binary.\n");
		return -1;
	}

	return 0;
}

/* ================================================================
 * Operator confirmation
 * ================================================================ */

static void print_schedule(const schedule_t *sched)
{
	static const char *day_names[] = {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	};

	switch (sched->type) {
	case SCHED_WEEKLY:
		printf("    Schedule:    Every %s at %02d:%02d\n",
		       day_names[sched->day_of_week], sched->hour, sched->minute);
		break;
	case SCHED_DAILY:
		printf("    Schedule:    Daily at %02d:%02d\n",
		       sched->hour, sched->minute);
		break;
	case SCHED_INTERVAL:
		printf("    Schedule:    Every %d seconds\n", sched->interval_secs);
		break;
	}
}

static int confirm_config(void)
{
	int i;
	char response[16];

	printf("\n");
	printf("  Store Auto-Maintenance Configuration:\n");
	printf("  ======================================\n");
	printf("    XML Config:  %s\n", g_opts.xml_config_path);
	ume_config_print(stdout, &g_config);
	print_schedule(&g_schedule);
	printf("    Timestamps:  %s\n", g_opts.utc_timestamps ? "UTC" : "local time");
	printf("    Keyword:     %s (threshold: %d in %ds)\n",
	       g_opts.keyword_str, g_opts.keyword_threshold, g_opts.keyword_window);
	printf("    Shutdown:    %ds graceful -> %ds escalated -> SIGKILL\n",
	       g_opts.shutdown_wait, SHUTDOWN_ESCALATE_WAIT);
	printf("    Backup dir:  %s\n", g_opts.backup_dir);
	printf("    Maint log:   %s\n", g_opts.maint_log_path);
	printf("    umestored:   %s\n", g_opts.umestored_path);
	if (g_opts.umestored_extra_argc > 0) {
		int j;
		printf("    umestored args:");
		for (j = 0; j < g_opts.umestored_extra_argc; j++)
			printf(" %s", g_opts.umestored_extra_argv[j]);
		printf("\n");
	}
	printf("    umesnaprepo: %s\n", g_opts.umesnaprepo_path);

	if (g_opts.store_filter[0]) {
		printf("    Store filter: %s only\n", g_opts.store_filter);
	}

	printf("\n    Stores to maintain:\n");
	for (i = 0; i < g_config.store_count; i++) {
		if (g_opts.store_filter[0] &&
		    strcmp(g_opts.store_filter, g_config.stores[i].name) != 0) {
			continue;
		}
		printf("      [%d] %-20s state=%s  cache=%s\n",
		       i + 1,
		       g_config.stores[i].name,
		       g_config.stores[i].state_dir,
		       g_config.stores[i].cache_dir[0] ? g_config.stores[i].cache_dir : "(none)");
	}

	if (g_opts.auto_confirm) {
		printf("\n    Auto-confirm enabled (-y), proceeding.\n\n");
		return 0;
	}

	printf("\n    Proceed? [Y/n] ");
	fflush(stdout);

	if (fgets(response, sizeof(response), stdin) == NULL)
		return -1;

	/* Trim */
	{
		char *p = response;
		while (*p && isspace((unsigned char)*p)) p++;
		if (*p == '\0' || *p == 'y' || *p == 'Y')
			return 0;
	}

	return -1;
}

/* ================================================================
 * Keyword scanner
 * ================================================================ */

static void keyword_ring_init(keyword_ring_t *ring)
{
	memset(ring, 0, sizeof(*ring));
}

static void keyword_ring_add(keyword_ring_t *ring, time_t ts)
{
	ring->timestamps[ring->head] = ts;
	ring->head = (ring->head + 1) % MAX_KEYWORD_EVENTS;
	if (ring->count < MAX_KEYWORD_EVENTS)
		ring->count++;
}

/* Count events within the last window_secs seconds */
static int keyword_ring_count_recent(keyword_ring_t *ring, time_t now, int window_secs)
{
	int count = 0;
	int i;
	time_t cutoff = now - window_secs;

	for (i = 0; i < ring->count; i++) {
		int idx = (ring->head - 1 - i + MAX_KEYWORD_EVENTS) % MAX_KEYWORD_EVENTS;
		if (ring->timestamps[idx] >= cutoff)
			count++;
		else
			break; /* timestamps are ordered, older ones follow */
	}
	return count;
}

/*
 * UME log lines carry a bracketed severity tag, e.g. [ERROR], [WARNING],
 * [ALERT], [CRIT].  Match these prefixes (open-bracket + keyword, no closing
 * bracket) so that [ERROR] lines are caught by "err" and [WARNING] by "warn",
 * while also handling compact forms like [WARN] and [ERR] and extended forms
 * like [CRITICAL].
 *
 * Using open-bracket prefix (without closing bracket) intentionally:
 *   "[err" matches "[ERROR]", "[error]", "[ERR]"  — all are errors.
 *   "[warn" matches "[WARNING]", "[WARN]"          — all are warnings.
 */
static const char *severity_tag_prefixes[] = {
	"[warn",    /* SEV_WARN:  [warn], [warning]  */
	"[err",     /* SEV_ERR:   [error], [err]     */
	"[alert",   /* SEV_ALERT: [alert]            */
	"[crit",    /* SEV_CRIT:  [crit], [critical] */
};

/* Check a log line for severity keywords at or above the minimum level.
 * Returns 1 if a matching keyword is found.
 *
 * When the line contains a UME-style [LEVEL] tag, match against the tag only.
 * This prevents false positives where a keyword appears in the message body
 * (e.g. "[ERROR]: no interfaces matching crit ..." with -k crit set).
 * Lines with no structured severity tag fall back to bare substring matching. */
static int scan_line_for_keywords(const char *line)
{
	int i;
	char lower_line[LOG_LINE_MAX];
	int len;
	int has_severity_tag = 0;

	/* Convert line to lowercase for case-insensitive matching */
	len = (int)strlen(line);
	if (len >= LOG_LINE_MAX) len = LOG_LINE_MAX - 1;
	for (i = 0; i < len; i++)
		lower_line[i] = tolower((unsigned char)line[i]);
	lower_line[len] = '\0';

	/* Detect whether the line carries a UME-style [LEVEL] severity tag */
	for (i = 0; i < SEV_COUNT; i++) {
		if (strstr(lower_line, severity_tag_prefixes[i]) != NULL) {
			has_severity_tag = 1;
			break;
		}
	}

	if (has_severity_tag) {
		/* Structured log line: match only against the [LEVEL] tag */
		for (i = g_min_severity; i < SEV_COUNT; i++) {
			if (strstr(lower_line, severity_tag_prefixes[i]) != NULL)
				return 1;
		}
	} else {
		/* Unstructured output: require word boundaries around the keyword
		 * so e.g. "crit" doesn't match inside "criteria". */
		for (i = g_min_severity; i < SEV_COUNT; i++) {
			const char *name = severity_names[i];
			size_t name_len = strlen(name);
			const char *p = lower_line;
			while ((p = strstr(p, name)) != NULL) {
				int left_ok = (p == lower_line) || !isalpha((unsigned char)p[-1]);
				int right_ok = !isalpha((unsigned char)p[name_len]);
				if (left_ok && right_ok) return 1;
				p += name_len;
			}
		}
	}
	return 0;
}

/* Process a line from the child's output.
 * Returns 1 if keyword threshold has been exceeded. */
static int process_child_line(const char *line)
{
	time_t now = time(NULL);
	int recent;

	/* Always echo child output to stdout and maintenance log */
	fprintf(stdout, "[child] %s\n", line);
	fflush(stdout);
	if (g_log_fp) {
		fprintf(g_log_fp, "[child] %s\n", line);
		fflush(g_log_fp);
	}

	if (scan_line_for_keywords(line)) {
		keyword_ring_add(&g_keyword_ring, now);
		LOG_WARN("Keyword match in child output: %s", line);

		recent = keyword_ring_count_recent(&g_keyword_ring, now,
		                                    g_opts.keyword_window);
		if (recent >= g_opts.keyword_threshold) {
			LOG_ERROR("Keyword threshold exceeded: %d occurrences in %d seconds",
			          recent, g_opts.keyword_window);
			return 1;
		}
	}
	return 0;
}

/* ================================================================
 * Child process management
 * ================================================================ */

/* Forward declarations (defined in Maintenance execution section) */
static size_t path_dir_len(const char *path);
static void resolve_config_path(const char *path, char *out, size_t out_size);
static int script_exists_in(const char *dir);

#ifndef _WIN32

/* ================================================================
 * PID file management
 * ================================================================ */

static void write_pid_file(pid_t pid)
{
	FILE *fp = fopen(g_opts.pid_file_path, "w");
	if (!fp) {
		LOG_WARN("Cannot write PID file %s: %s",
		         g_opts.pid_file_path, strerror(errno));
		return;
	}
	fprintf(fp, "%d\n", (int)pid);
	fclose(fp);
}

static void remove_pid_file(void)
{
	if (g_opts.pid_file_path[0])
		unlink(g_opts.pid_file_path);
}

/* Check for a running umestored from a previous (possibly crashed) session.
 * Returns 0 if safe to proceed, -1 if a live process was found. */
static int check_existing_pid_file(void)
{
	FILE *fp;
	int stored_pid;
	char abs_pid[MAX_PATH_LEN];

	resolve_config_path(g_opts.pid_file_path, abs_pid, sizeof(abs_pid));
	snprintf(g_opts.pid_file_path, MAX_PATH_LEN, "%s", abs_pid);

	fp = fopen(g_opts.pid_file_path, "r");
	if (!fp)
		return 0; /* no PID file — clean start */

	if (fscanf(fp, "%d", &stored_pid) != 1) {
		fclose(fp);
		LOG_WARN("PID file %s is malformed — removing.", g_opts.pid_file_path);
		remove_pid_file();
		return 0;
	}
	fclose(fp);

	/* kill(pid, 0) checks if the process exists without sending a signal */
	if (kill((pid_t)stored_pid, 0) == 0) {
		LOG_ERROR("PID file %s exists and PID %d is still running.",
		          g_opts.pid_file_path, stored_pid);
		LOG_ERROR("A previously managed umestored may still be active.");
		LOG_ERROR("Stop it manually or remove the PID file to proceed.");
		return -1;
	}

	/* Process no longer exists — stale PID file */
	LOG_WARN("Stale PID file %s (PID %d no longer running) — removing.",
	         g_opts.pid_file_path, stored_pid);
	remove_pid_file();
	return 0;
}

static int start_child(void)
{
	if (pipe(g_pipe_fd) < 0) {
		LOG_ERROR("pipe() failed: %s", strerror(errno));
		return -1;
	}

	g_child_pid = fork();
	if (g_child_pid < 0) {
		LOG_ERROR("fork() failed: %s", strerror(errno));
		close(g_pipe_fd[0]);
		close(g_pipe_fd[1]);
		return -1;
	}

	if (g_child_pid == 0) {
		/* Child process */
		close(g_pipe_fd[0]); /* close read end */

		/* Put the child in its own process group so terminal signals
		 * (Ctrl+C) do not reach umestored directly. store_auto_maint
		 * controls all signals to the child via stop_child(). */
		setpgid(0, 0);

		/* Redirect stdout and stderr to the pipe */
		dup2(g_pipe_fd[1], STDOUT_FILENO);
		dup2(g_pipe_fd[1], STDERR_FILENO);
		close(g_pipe_fd[1]);

		/* chdir to the XML config's directory so relative paths inside
		 * the config (e.g. lbm_store.cfg) resolve correctly. */
		{
			char config_copy[MAX_PATH_LEN];
			strncpy(config_copy, g_opts.xml_config_path, MAX_PATH_LEN - 1);
			config_copy[MAX_PATH_LEN - 1] = '\0';
			char *config_dir = dirname(config_copy);
			if (chdir(config_dir) != 0) {
				fprintf(stderr, "ERROR: chdir to config dir '%s' failed: %s\n",
				        config_dir, strerror(errno));
				_exit(127);
			}
		}

		/* Build argv for umestored: [path, extra_opts..., configfile, NULL]
		 * Pass only the basename of the config since we chdir'd to its dir. */
		{
			char config_copy2[MAX_PATH_LEN];
			strncpy(config_copy2, g_opts.xml_config_path, MAX_PATH_LEN - 1);
			config_copy2[MAX_PATH_LEN - 1] = '\0';
			char *config_base = basename(config_copy2);

			int i, nargs = 2 + g_opts.umestored_extra_argc; /* path + extras + configfile */
			char **child_argv = (char **)malloc((nargs + 1) * sizeof(char *));
			int idx = 0;

			child_argv[idx++] = g_opts.umestored_path;
			for (i = 0; i < g_opts.umestored_extra_argc; i++)
				child_argv[idx++] = g_opts.umestored_extra_argv[i];
			child_argv[idx++] = config_base;
			child_argv[idx] = NULL;

			execvp(g_opts.umestored_path, child_argv);
			/* free not needed — exec replaces the process */
		}

		/* If we get here, exec failed */
		fprintf(stderr, "ERROR: exec %s failed: %s\n",
		        g_opts.umestored_path, strerror(errno));
		_exit(127);
	}

	/* Parent process */
	close(g_pipe_fd[1]); /* close write end */
	g_pipe_fd[1] = -1;

	/* Set read end to non-blocking */
	{
		int flags = fcntl(g_pipe_fd[0], F_GETFL, 0);
		fcntl(g_pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
	}

	g_child_alive = 1;
	write_pid_file(g_child_pid);
	LOG_INFO("Started umestored (PID %d) with config %s",
	         g_child_pid, g_opts.xml_config_path);
	LOG_INFO("PID file: %s", g_opts.pid_file_path);

	return 0;
}

/* Helper: wait for child exit up to timeout_secs. Returns 1 if exited, 0 otherwise.
 * If honor_fast_shutdown is nonzero, also returns 0 early when g_fast_shutdown
 * is set so phase 1 can short-circuit on a 2nd Ctrl-C. */
static int wait_for_child_exit_ex(int timeout_secs, int *exit_status, int honor_fast_shutdown)
{
	int elapsed = 0;
	int status;

	while (elapsed < timeout_secs) {
		if (waitpid(g_child_pid, &status, WNOHANG) > 0) {
			if (exit_status) *exit_status = WEXITSTATUS(status);
			return 1;
		}
		if (honor_fast_shutdown && g_fast_shutdown) return 0;
		sleep(1);
		elapsed++;
		if (elapsed % 60 == 0) {
			LOG_INFO("Shutdown in progress... %d/%d seconds elapsed", elapsed, timeout_secs);
		}
	}
	return 0;
}

static int wait_for_child_exit(int timeout_secs, int *exit_status)
{
	return wait_for_child_exit_ex(timeout_secs, exit_status, 0);
}

static void cleanup_child_state(void)
{
	g_child_alive = 0;
	g_child_pid = 0;
	remove_pid_file();
	if (g_pipe_fd[0] >= 0) {
		close(g_pipe_fd[0]);
		g_pipe_fd[0] = -1;
	}
}

static int stop_child(void)
{
	int exit_status = -1;
	time_t t_start, t_end;
	double elapsed_secs;

	if (!g_child_alive || g_child_pid <= 0)
		return 0;

	t_start = time(NULL);
	LOG_INFO("Stopping umestored (PID %d)...", g_child_pid);

	/* Phase 1: Single SIGINT, wait up to shutdown_wait seconds (default 1 hour).
	 * Skipped if operator already requested fast shutdown (2x Ctrl-C). The phase-1
	 * wait also short-circuits if a 2nd Ctrl-C arrives while we're waiting. */
	if (!g_fast_shutdown) {
		LOG_INFO("Phase 1: Sending SIGINT, waiting up to %d seconds for graceful shutdown...",
		         g_opts.shutdown_wait);
		kill(g_child_pid, SIGINT);

		if (wait_for_child_exit_ex(g_opts.shutdown_wait, &exit_status, 1)) {
			t_end = time(NULL);
			elapsed_secs = difftime(t_end, t_start);
			LOG_INFO("umestored exited (status %d) after %.0f seconds (phase 1 - single SIGINT)",
			         exit_status, elapsed_secs);
			cleanup_child_state();
			return 0;
		}
	}

	/* Phase 2: Two SIGINTs in succession, wait 10 more minutes */
	if (g_fast_shutdown) {
		LOG_WARN("Phase 2: Operator requested fast shutdown. Sending two SIGINTs, "
		         "waiting up to %d seconds...", SHUTDOWN_ESCALATE_WAIT);
	} else {
		LOG_WARN("Phase 2: Store did not exit after %d seconds. Sending two SIGINTs, "
		         "waiting up to %d seconds...", g_opts.shutdown_wait, SHUTDOWN_ESCALATE_WAIT);
	}
	kill(g_child_pid, SIGINT);
	kill(g_child_pid, SIGINT);

	if (wait_for_child_exit(SHUTDOWN_ESCALATE_WAIT, &exit_status)) {
		t_end = time(NULL);
		elapsed_secs = difftime(t_end, t_start);
		LOG_INFO("umestored exited (status %d) after %.0f seconds (phase 2 - double SIGINT)",
		         exit_status, elapsed_secs);
		cleanup_child_state();
		return 0;
	}

	/* Phase 3: SIGKILL as last resort */
	t_end = time(NULL);
	elapsed_secs = difftime(t_end, t_start);
	LOG_WARN("Phase 3: Store did not exit after %.0f seconds total. Sending SIGKILL.",
	         elapsed_secs);
	kill(g_child_pid, SIGKILL);
	waitpid(g_child_pid, &exit_status, 0);

	t_end = time(NULL);
	elapsed_secs = difftime(t_end, t_start);
	LOG_WARN("umestored killed (SIGKILL) after %.0f seconds total", elapsed_secs);
	cleanup_child_state();
	return 0;
}

/* Read available data from the child pipe and process line by line.
 * Returns 1 if keyword threshold exceeded, 0 otherwise. */
static int read_child_output(void)
{
	static char linebuf[LOG_LINE_MAX];
	static int linepos = 0;
	char buf[PIPE_BUF_SIZE];
	ssize_t n;
	int threshold_hit = 0;

	if (g_pipe_fd[0] < 0)
		return 0;

	while ((n = read(g_pipe_fd[0], buf, sizeof(buf) - 1)) > 0) {
		int i;
		buf[n] = '\0';
		for (i = 0; i < n; i++) {
			if (buf[i] == '\n' || buf[i] == '\r') {
				if (linepos > 0) {
					linebuf[linepos] = '\0';
					if (process_child_line(linebuf))
						threshold_hit = 1;
					linepos = 0;
				}
			} else if (linepos < LOG_LINE_MAX - 1) {
				linebuf[linepos++] = buf[i];
			}
		}
	}

	return threshold_hit;
}

/* Check if the child process is still running */
static int check_child_alive(void)
{
	int status;
	if (g_child_pid <= 0)
		return 0;
	if (waitpid(g_child_pid, &status, WNOHANG) > 0) {
		LOG_WARN("umestored (PID %d) exited unexpectedly (status %d)",
		         g_child_pid, WEXITSTATUS(status));
		g_child_alive = 0;
		g_child_pid = 0;
		if (g_pipe_fd[0] >= 0) {
			close(g_pipe_fd[0]);
			g_pipe_fd[0] = -1;
		}
		return 0;
	}
	return 1;
}

#else /* _WIN32 */

static int start_child(void)
{
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	char cmdline[MAX_PATH_LEN * 2];

	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&g_pipe_read, &g_pipe_write, &sa, 0)) {
		LOG_ERROR("CreatePipe failed: %lu", GetLastError());
		return -1;
	}
	SetHandleInformation(g_pipe_read, HANDLE_FLAG_INHERIT, 0);

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.hStdOutput = g_pipe_write;
	si.hStdError = g_pipe_write;
	si.dwFlags |= STARTF_USESTDHANDLES;

	{
		int i, off;
		off = snprintf(cmdline, sizeof(cmdline), "%s", g_opts.umestored_path);
		for (i = 0; i < g_opts.umestored_extra_argc && off < (int)sizeof(cmdline) - 1; i++)
			off += snprintf(cmdline + off, sizeof(cmdline) - off, " %s",
			                g_opts.umestored_extra_argv[i]);
		snprintf(cmdline + off, sizeof(cmdline) - off, " %s", g_opts.xml_config_path);
	}

	/* Derive working directory from XML config path so relative paths inside
	 * the config (e.g. lbm_store.cfg) resolve correctly — mirrors the chdir()
	 * done in the POSIX child. */
	{
		char config_dir[MAX_PATH_LEN];
		size_t dir_len = path_dir_len(g_opts.xml_config_path);
		if (dir_len > 0)
			snprintf(config_dir, sizeof(config_dir), "%.*s",
			         (int)dir_len, g_opts.xml_config_path);
		else
			snprintf(config_dir, sizeof(config_dir), ".");

		/* CREATE_NEW_PROCESS_GROUP isolates the child in its own process group
		 * so that GenerateConsoleCtrlEvent targets only the child, not the
		 * parent — equivalent to setpgid(0,0) on POSIX.
		 * Note: CTRL_C_EVENT is ignored for CREATE_NEW_PROCESS_GROUP children;
		 * use CTRL_BREAK_EVENT instead (see stop_child). */
		if (!CreateProcess(NULL, cmdline, NULL, NULL, TRUE,
		                   CREATE_NEW_PROCESS_GROUP, NULL, config_dir, &si, &pi)) {
			LOG_ERROR("CreateProcess failed: %lu", GetLastError());
			CloseHandle(g_pipe_read);
			CloseHandle(g_pipe_write);
			return -1;
		}
	}

	CloseHandle(g_pipe_write);
	g_pipe_write = INVALID_HANDLE_VALUE;

	g_child_handle = pi.hProcess;
	g_child_pid = pi.dwProcessId;
	CloseHandle(pi.hThread);

	g_child_alive = 1;
	LOG_INFO("Started umestored (PID %lu) with config %s",
	         (unsigned long)g_child_pid, g_opts.xml_config_path);

	return 0;
}

static void cleanup_child_state_win32(void)
{
	CloseHandle(g_child_handle);
	g_child_handle = INVALID_HANDLE_VALUE;
	g_child_alive = 0;
	g_child_pid = 0;
	if (g_pipe_read != INVALID_HANDLE_VALUE) {
		CloseHandle(g_pipe_read);
		g_pipe_read = INVALID_HANDLE_VALUE;
	}
}

static int stop_child(void)
{
	DWORD exit_code;
	DWORD wait_result;
	time_t t_start, t_end;
	double elapsed_secs;
	DWORD phase1_ms = (DWORD)g_opts.shutdown_wait * 1000;
	DWORD phase2_ms = SHUTDOWN_ESCALATE_WAIT * 1000;

	if (!g_child_alive || g_child_handle == INVALID_HANDLE_VALUE)
		return 0;

	t_start = time(NULL);
	LOG_INFO("Stopping umestored (PID %lu)...", (unsigned long)g_child_pid);

	/* Phase 1: Ctrl+Break, wait up to shutdown_wait seconds (default 1 hour).
	 * CTRL_BREAK_EVENT is used (not CTRL_C_EVENT) because the child was created
	 * with CREATE_NEW_PROCESS_GROUP, which causes CTRL_C_EVENT to be ignored.
	 * Skipped entirely if operator already requested fast shutdown (2x Ctrl-C). */
	if (!g_fast_shutdown) {
		LOG_INFO("Phase 1: Sending Ctrl+Break, waiting up to %d seconds for graceful shutdown...",
		         g_opts.shutdown_wait);
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, g_child_pid);

		/* Poll in 1s slices so a 2nd Ctrl-C during phase-1 short-circuits */
		DWORD waited_ms = 0;
		wait_result = WAIT_TIMEOUT;
		while (waited_ms < phase1_ms && !g_fast_shutdown) {
			wait_result = WaitForSingleObject(g_child_handle, 1000);
			if (wait_result != WAIT_TIMEOUT) break;
			waited_ms += 1000;
		}
		if (wait_result != WAIT_TIMEOUT) {
			GetExitCodeProcess(g_child_handle, &exit_code);
			t_end = time(NULL);
			elapsed_secs = difftime(t_end, t_start);
			LOG_INFO("umestored exited (code %lu) after %.0f seconds (phase 1 - single Ctrl+Break)",
			         (unsigned long)exit_code, elapsed_secs);
			cleanup_child_state_win32();
			return 0;
		}
	}

	/* Phase 2: Two Ctrl+Break events, wait 10 more minutes */
	if (g_fast_shutdown) {
		LOG_WARN("Phase 2: Operator requested fast shutdown. Sending two Ctrl+Break events, "
		         "waiting up to %d seconds...", SHUTDOWN_ESCALATE_WAIT);
	} else {
		LOG_WARN("Phase 2: Store did not exit after %d seconds. Sending two Ctrl+Break events, "
		         "waiting up to %d seconds...", g_opts.shutdown_wait, SHUTDOWN_ESCALATE_WAIT);
	}
	GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, g_child_pid);
	GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, g_child_pid);

	wait_result = WaitForSingleObject(g_child_handle, phase2_ms);
	if (wait_result != WAIT_TIMEOUT) {
		GetExitCodeProcess(g_child_handle, &exit_code);
		t_end = time(NULL);
		elapsed_secs = difftime(t_end, t_start);
		LOG_INFO("umestored exited (code %lu) after %.0f seconds (phase 2 - double Ctrl+Break)",
		         (unsigned long)exit_code, elapsed_secs);
		cleanup_child_state_win32();
		return 0;
	}

	/* Phase 3: TerminateProcess as last resort */
	t_end = time(NULL);
	elapsed_secs = difftime(t_end, t_start);
	LOG_WARN("Phase 3: Store did not exit after %.0f seconds total. Terminating process.",
	         elapsed_secs);
	TerminateProcess(g_child_handle, 1);
	WaitForSingleObject(g_child_handle, 5000);

	GetExitCodeProcess(g_child_handle, &exit_code);
	t_end = time(NULL);
	elapsed_secs = difftime(t_end, t_start);
	LOG_WARN("umestored terminated (code %lu) after %.0f seconds total",
	         (unsigned long)exit_code, elapsed_secs);
	cleanup_child_state_win32();
	return 0;
}

static int read_child_output(void)
{
	static char linebuf[LOG_LINE_MAX];
	static int linepos = 0;
	char buf[PIPE_BUF_SIZE];
	DWORD n, avail;
	int threshold_hit = 0;

	if (g_pipe_read == INVALID_HANDLE_VALUE)
		return 0;

	while (PeekNamedPipe(g_pipe_read, NULL, 0, NULL, &avail, NULL) && avail > 0) {
		DWORD to_read = (avail < sizeof(buf) - 1) ? avail : sizeof(buf) - 1;
		if (!ReadFile(g_pipe_read, buf, to_read, &n, NULL) || n == 0)
			break;
		buf[n] = '\0';
		{
			DWORD i;
			for (i = 0; i < n; i++) {
				if (buf[i] == '\n' || buf[i] == '\r') {
					if (linepos > 0) {
						linebuf[linepos] = '\0';
						if (process_child_line(linebuf))
							threshold_hit = 1;
						linepos = 0;
					}
				} else if (linepos < LOG_LINE_MAX - 1) {
					linebuf[linepos++] = buf[i];
				}
			}
		}
	}

	return threshold_hit;
}

static int check_child_alive(void)
{
	DWORD exit_code;
	if (g_child_handle == INVALID_HANDLE_VALUE)
		return 0;
	if (GetExitCodeProcess(g_child_handle, &exit_code)) {
		if (exit_code != STILL_ACTIVE) {
			LOG_WARN("umestored (PID %lu) exited unexpectedly (code %lu)",
			         (unsigned long)g_child_pid, (unsigned long)exit_code);
			CloseHandle(g_child_handle);
			g_child_handle = INVALID_HANDLE_VALUE;
			g_child_alive = 0;
			g_child_pid = 0;
			if (g_pipe_read != INVALID_HANDLE_VALUE) {
				CloseHandle(g_pipe_read);
				g_pipe_read = INVALID_HANDLE_VALUE;
			}
			return 0;
		}
	}
	return 1;
}

#endif /* _WIN32 */

/* ================================================================
 * Log file monitoring (when log goes to a file, not pipe)
 * ================================================================ */

static FILE *g_logfile_fp = NULL;

static int open_store_logfile(void)
{
	char abs_log[MAX_PATH_LEN];

	if (!g_config.log_configured)
		return 0; /* no log file, will use pipe */

	resolve_config_path(g_config.log_path, abs_log, sizeof(abs_log));

	g_logfile_fp = fopen(abs_log, "r");
	if (!g_logfile_fp) {
		LOG_WARN("Cannot open store log file: %s (will retry later)", abs_log);
		return 0; /* non-fatal, file may not exist yet */
	}

	/* Seek to end to only monitor new output */
	fseek(g_logfile_fp, 0, SEEK_END);
	LOG_INFO("Monitoring store log file: %s", abs_log);
	return 0;
}

static int read_store_logfile(void)
{
	char line[LOG_LINE_MAX];
	int threshold_hit = 0;

	if (!g_logfile_fp) {
		/* Try to open if it wasn't available before */
		if (g_config.log_configured) {
			char abs_log[MAX_PATH_LEN];
			resolve_config_path(g_config.log_path, abs_log, sizeof(abs_log));
			g_logfile_fp = fopen(abs_log, "r");
			if (g_logfile_fp)
				fseek(g_logfile_fp, 0, SEEK_END);
		}
		return 0;
	}

	while (fgets(line, sizeof(line), g_logfile_fp) != NULL) {
		/* Strip newline */
		int len = (int)strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';
		if (len > 0) {
			if (process_child_line(line))
				threshold_hit = 1;
		}
	}

	/* Clear EOF so we can read new data next time */
	clearerr(g_logfile_fp);

	return threshold_hit;
}

/* ================================================================
 * Maintenance execution
 * ================================================================ */

/* Portable dirname: return length of the directory portion of a path.
 * Scans backwards for the last '/' or '\\' separator. */
static size_t path_dir_len(const char *path)
{
	size_t len = strlen(path);
	while (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\')
		len--;
	/* Strip trailing separator, but keep root "/" or "C:\\" */
	if (len > 1)
		len--;
	return len;
}

/* Resolve path relative to the XML config's directory if not absolute.
 * Result written into 'out' buffer of size 'out_size'. */
static void resolve_config_path(const char *path, char *out, size_t out_size)
{
	/* Absolute path check: starts with '/', '\\', or a drive letter (e.g. "C:\") */
	int is_absolute = (path[0] == '/' || path[0] == '\\' ||
	                   (path[0] != '\0' && path[1] == ':'));

	if (is_absolute || path[0] == '\0') {
		/* Already absolute or empty — use as-is */
		strncpy(out, path, out_size - 1);
		out[out_size - 1] = '\0';
	} else {
		/* Relative — prepend the XML config's directory */
		size_t dir_len = path_dir_len(g_opts.xml_config_path);
		if (dir_len == 0) {
			/* Config is in current directory, no prefix needed */
			strncpy(out, path, out_size - 1);
			out[out_size - 1] = '\0';
		} else {
			snprintf(out, out_size, "%.*s%c%s",
			         (int)dir_len, g_opts.xml_config_path, PATH_SEP, path);
		}
	}
}

/* Build and execute the maintenance script command */
static int run_maintenance_script(void)
{
	char cmd[MAX_PATH_LEN * 16];
	int i, offset;
	int rc;

	/* Verify script exists before attempting to run it */
	if (!script_exists_in(g_opts.maint_script_dir)) {
		LOG_ERROR("Maintenance script not found in '%s'. "
		          "Use -R DIR to specify its location.",
		          g_opts.maint_script_dir);
		return 127;
	}

	/*
	 * Build command: maintain_store.sh -r REPO -d BACKUP -l LOG
	 *               -s STATE1 -c CACHE1 -n NAME1 [-s STATE2 ...]
	 */
	offset = snprintf(cmd, sizeof(cmd),
	         "%s" PATH_SEP_STR "maintain_store" SCRIPT_EXT
	         " -r \"%s\" -d \"%s\" -l \"%s\"%s",
	         g_opts.maint_script_dir,
	         g_opts.umesnaprepo_path,
	         g_opts.backup_dir,
	         g_opts.maint_log_path,
	         g_opts.utc_timestamps ? " -u" : "");

	for (i = 0; i < g_config.store_count; i++) {
		char abs_state[MAX_PATH_LEN];
		char abs_cache[MAX_PATH_LEN];

		if (g_opts.store_filter[0] &&
		    strcmp(g_opts.store_filter, g_config.stores[i].name) != 0) {
			continue;
		}
		resolve_config_path(g_config.stores[i].state_dir, abs_state, sizeof(abs_state));
		resolve_config_path(g_config.stores[i].cache_dir, abs_cache, sizeof(abs_cache));
		offset += snprintf(cmd + offset, sizeof(cmd) - offset,
		         " -s \"%s\" -c \"%s\" -n \"%s\"",
		         abs_state,
		         abs_cache,
		         g_config.stores[i].name);
	}

	LOG_INFO("Running maintenance script: %s", cmd);

	/*
	 * On Windows, fopen("a") holds an exclusive write lock on the log
	 * file.  The maintenance script also opens the same file for append
	 * (echo >> LOG_FILE) and would fail with "file is being used by
	 * another process."  Release the handle before launching the script
	 * and reopen it afterwards.  Safe here because system() blocks this
	 * process for the duration of the script.
	 */
	if (g_log_fp) {
		fflush(g_log_fp);
		fclose(g_log_fp);
		g_log_fp = NULL;
	}

	rc = system(cmd);

	/* Reopen the maintenance log in append mode after the script exits */
	if (g_opts.maint_log_path[0]) {
		g_log_fp = fopen(g_opts.maint_log_path, "a");
	}

#ifndef _WIN32
	if (WIFEXITED(rc))
		rc = WEXITSTATUS(rc);
#endif

	if (rc != 0) {
		LOG_ERROR("Maintenance script failed with exit code %d", rc);
	} else {
		LOG_INFO("Maintenance script completed successfully");
	}

	return rc;
}

/* Copy files from backup directory back to original locations */
static int revert_from_backup(void)
{
	char cmd[MAX_PATH_LEN * 4];
	int i;

	LOG_INFO("Reverting to backup files from %s", g_opts.backup_dir);

	for (i = 0; i < g_config.store_count; i++) {
		if (g_opts.store_filter[0] &&
		    strcmp(g_opts.store_filter, g_config.stores[i].name) != 0)
			continue;

		/* Copy backed-up state files back */
#ifndef _WIN32
		snprintf(cmd, sizeof(cmd),
		         "cp -f \"%s/%s/\"*state* \"%s/\" 2>/dev/null; "
		         "cp -f \"%s/%s/\"*cache* \"%s/\" 2>/dev/null",
		         g_opts.backup_dir, g_config.stores[i].name,
		         g_config.stores[i].state_dir,
		         g_opts.backup_dir, g_config.stores[i].name,
		         g_config.stores[i].cache_dir);
#else
		snprintf(cmd, sizeof(cmd),
		         "copy /Y \"%s\\%s\\*state*\" \"%s\\\" >nul 2>&1 & "
		         "copy /Y \"%s\\%s\\*cache*\" \"%s\\\" >nul 2>&1",
		         g_opts.backup_dir, g_config.stores[i].name,
		         g_config.stores[i].state_dir,
		         g_opts.backup_dir, g_config.stores[i].name,
		         g_config.stores[i].cache_dir);
#endif
		LOG_INFO("Reverting store %s: %s", g_config.stores[i].name, cmd);
		system(cmd);
	}

	return 0;
}

/* Wipe state and cache directories for a fresh start */
static int wipe_directories(void)
{
	char cmd[MAX_PATH_LEN * 4];
	int i;

	LOG_WARN("Wiping state and cache directories for fresh start");

	for (i = 0; i < g_config.store_count; i++) {
		if (g_opts.store_filter[0] &&
		    strcmp(g_opts.store_filter, g_config.stores[i].name) != 0)
			continue;

#ifndef _WIN32
		snprintf(cmd, sizeof(cmd),
		         "rm -f \"%s/\"* 2>/dev/null; rm -f \"%s/\"* 2>/dev/null",
		         g_config.stores[i].state_dir,
		         g_config.stores[i].cache_dir);
#else
		snprintf(cmd, sizeof(cmd),
		         "del /Q \"%s\\*\" >nul 2>&1 & del /Q \"%s\\*\" >nul 2>&1",
		         g_config.stores[i].state_dir,
		         g_config.stores[i].cache_dir);
#endif
		LOG_WARN("Wiping store %s: %s", g_config.stores[i].name, cmd);
		system(cmd);
	}

	return 0;
}

/* Perform maintenance with 3-tier recovery */
static int perform_maintenance(void)
{
	int attempt;
	int script_rc;

	g_stats.initiated++;
	LOG_INFO("=== Maintenance cycle #%d initiated ===", g_stats.initiated);

	/* Stop the child process */
	stop_child();

#ifdef _WIN32
	/*
	 * Windows does not guarantee that a process's file handles are
	 * fully released the instant WaitForSingleObject() returns.
	 * The kernel completes handle teardown asynchronously; on a
	 * fast exit (e.g. 0-second Ctrl+Break response) the store cache
	 * files can still be locked for a brief window.  A short pause
	 * eliminates the race between umestored exit and umesnaprepo
	 * opening the same files.  2 seconds is conservative; typical
	 * release latency is < 100 ms.
	 */
	Sleep(2000);
#endif

	/* Close log file handle so umesnaprepo can work */
	if (g_logfile_fp) {
		fclose(g_logfile_fp);
		g_logfile_fp = NULL;
	}

	/* Run the maintenance script */
	script_rc = run_maintenance_script();

	/* 3-tier restart attempts */
	for (attempt = 1; attempt <= CHILD_RESTART_MAX_ATTEMPTS; attempt++) {

		if (attempt == 2 && script_rc != 0) {
			/* Tier 2: Revert to backup */
			LOG_WARN("Tier 2: Reverting to backup files...");
			revert_from_backup();
		} else if (attempt == 3) {
			/* Tier 3: Wipe and start fresh */
			LOG_WARN("Tier 3: Wiping directories for fresh start...");
			wipe_directories();
		}

		LOG_INFO("Starting umestored (attempt %d/%d)...", attempt, CHILD_RESTART_MAX_ATTEMPTS);

		if (start_child() != 0) {
			LOG_ERROR("Failed to start umestored (attempt %d)", attempt);
			continue;
		}

		/* Wait a few seconds and check if child is still alive */
		sleep(CHILD_STARTUP_WAIT_SECS);

		/* Drain any output during startup */
		read_child_output();

		if (check_child_alive()) {
			LOG_INFO("umestored started successfully (attempt %d)", attempt);
			g_stats.completed++;
			if (script_rc != 0)
				g_stats.errors++;
			g_last_maint_time = time(NULL);

			/* Re-open log file monitor */
			open_store_logfile();

			/* Reset keyword ring after successful maintenance */
			keyword_ring_init(&g_keyword_ring);

			LOG_STATS("Maintenance: initiated=%d completed=%d errors=%d failed=%d",
			          g_stats.initiated, g_stats.completed, g_stats.errors, g_stats.failed);
			return 0;
		}

		LOG_ERROR("umestored failed to stay running (attempt %d)", attempt);
		stop_child(); /* clean up */
	}

	/* All attempts failed — stop the main loop */
	g_stats.failed++;
	LOG_ERROR("=== CRITICAL: All %d restart attempts failed! ===", CHILD_RESTART_MAX_ATTEMPTS);
	LOG_STATS("Maintenance: initiated=%d completed=%d errors=%d failed=%d",
	          g_stats.initiated, g_stats.completed, g_stats.errors, g_stats.failed);
	LOG_ERROR("Exiting — operator intervention required.");
	g_running = 0;

	return -1;
}

/* ================================================================
 * Scheduler
 * ================================================================ */

/* Check if now is the scheduled maintenance time.
 * For weekly/daily, returns 1 if we are within the target minute.
 * For interval, returns 1 if enough time has passed. */
static int is_maintenance_due(void)
{
	time_t now = time(NULL);
	struct tm *tm_now = localtime(&now);

	switch (g_schedule.type) {
	case SCHED_WEEKLY:
		if (tm_now->tm_wday == g_schedule.day_of_week &&
		    tm_now->tm_hour == g_schedule.hour &&
		    tm_now->tm_min == g_schedule.minute) {
			/* Only trigger once per minute window */
			if (now - g_last_maint_time > 120)
				return 1;
		}
		break;

	case SCHED_DAILY:
		if (tm_now->tm_hour == g_schedule.hour &&
		    tm_now->tm_min == g_schedule.minute) {
			if (now - g_last_maint_time > 120)
				return 1;
		}
		break;

	case SCHED_INTERVAL:
		if (g_last_maint_time == 0) {
			/* Don't trigger immediately on first startup */
			g_last_maint_time = now;
		} else if (now - g_last_maint_time >= g_schedule.interval_secs) {
			return 1;
		}
		break;
	}

	return 0;
}

/* ================================================================
 * Main loop
 * ================================================================ */

static int main_loop(void)
{
	int keyword_alert;

	LOG_INFO("Entering main loop. Waiting for schedule or keyword alerts.");

	while (g_running) {
		keyword_alert = 0;

		/* Read child output from pipe (always, for stderr) */
		if (read_child_output())
			keyword_alert = 1;

		/* Also read from store log file if configured */
		if (read_store_logfile())
			keyword_alert = 1;

		/* Check if child is still alive */
		if (g_child_alive && !check_child_alive()) {
			LOG_ERROR("Child process died unexpectedly");
		}

		/* Check if scheduled maintenance is due */
		if (is_maintenance_due()) {
			LOG_INFO("Scheduled maintenance time reached");
			perform_maintenance();
		}

		/* Keyword threshold exceeded — exit for operator intervention */
		if (keyword_alert) {
			LOG_ERROR("Keyword threshold exceeded — exiting. Operator intervention required.");
			g_running = 0;
		}

		/* Sleep briefly to avoid busy-waiting */
		sleep(1);
	}

	return 0;
}

/* ================================================================
 * Script directory resolution
 * ================================================================ */

/* Check if maintain_store script exists in a given directory.
 * Returns 1 if found. */
static int script_exists_in(const char *dir)
{
	char path[MAX_PATH_LEN];
	snprintf(path, sizeof(path), "%s%c%s%s",
	         dir, PATH_SEP, "maintain_store", SCRIPT_EXT);
#ifdef _WIN32
	return (_access(path, 0) == 0);
#else
	return (access(path, F_OK) == 0);
#endif
}

/* Resolve the maintenance script directory.
 * Priority: explicit -R option > binary's own dir > umesnaprepo dir > CWD.
 * Updates g_opts.maint_script_dir in place. */
static void resolve_script_dir(void)
{
	/* If user explicitly set -R, trust it as-is */
	if (strcmp(g_opts.maint_script_dir, DEFAULT_MAINT_SCRIPT_DIR) != 0)
		return;

#ifdef _WIN32
	/* Try the directory of this binary via GetModuleFileName */
	{
		char exe_path[MAX_PATH_LEN];
		DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
		if (len > 0 && len < sizeof(exe_path)) {
			size_t dir_len = path_dir_len(exe_path);
			if (dir_len > 0) {
				char bin_dir[MAX_PATH_LEN];
				snprintf(bin_dir, sizeof(bin_dir), "%.*s", (int)dir_len, exe_path);
				if (script_exists_in(bin_dir)) {
					snprintf(g_opts.maint_script_dir, MAX_PATH_LEN, "%s", bin_dir);
					return;
				}
			}
		}
	}
#else
	/* Try the directory of this binary via /proc/self/exe */
	{
		char exe_path[MAX_PATH_LEN];
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len > 0) {
			exe_path[len] = '\0';
			char exe_copy[MAX_PATH_LEN];
			strncpy(exe_copy, exe_path, MAX_PATH_LEN - 1);
			exe_copy[MAX_PATH_LEN - 1] = '\0';
			char *bin_dir = dirname(exe_copy);
			if (script_exists_in(bin_dir)) {
				strncpy(g_opts.maint_script_dir, bin_dir, MAX_PATH_LEN - 1);
				return;
			}
		}
	}
#endif

	/* Try the directory of umesnaprepo */
	if (g_opts.umesnaprepo_path[0] != '\0') {
		size_t dir_len = path_dir_len(g_opts.umesnaprepo_path);
		if (dir_len > 0) {
			char repo_dir[MAX_PATH_LEN];
			snprintf(repo_dir, sizeof(repo_dir), "%.*s",
			         (int)dir_len, g_opts.umesnaprepo_path);
			if (script_exists_in(repo_dir)) {
				snprintf(g_opts.maint_script_dir, MAX_PATH_LEN, "%s", repo_dir);
				return;
			}
		}
	}

	/* Fall back to CWD — will warn at maintenance time if not found */
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char **argv)
{
	/* Parse command-line options */
	if (parse_options(argc, argv) != 0)
		return 1;

	/* Resolve maintenance script directory */
	resolve_script_dir();

	/* Warn at startup if the maintenance script is missing — avoids
	 * discovering the problem only after umestored has been stopped. */
	if (!script_exists_in(g_opts.maint_script_dir)) {
		fprintf(stderr, "WARNING: Maintenance script (maintain_store%s) not found in '%s'.\n"
		                "         Use -R DIR to specify its location before maintenance runs.\n",
		                SCRIPT_EXT, g_opts.maint_script_dir);
	}

	/* Parse schedule */
	if (parse_schedule(g_opts.schedule_str, &g_schedule) != 0) {
		fprintf(stderr, "ERROR: Invalid schedule: %s\n", g_opts.schedule_str);
		return 1;
	}

	/* Parse severity level */
	g_min_severity = parse_severity(g_opts.keyword_str);

	/* Parse XML config */
	if (ume_config_parse(g_opts.xml_config_path, &g_config) != 0) {
		fprintf(stderr, "ERROR: Failed to parse XML config: %s\n",
		        g_opts.xml_config_path);
		return 1;
	}

	/* Validate store filter if specified */
	if (g_opts.store_filter[0]) {
		int found = 0;
		int i;
		for (i = 0; i < g_config.store_count; i++) {
			if (strcmp(g_opts.store_filter, g_config.stores[i].name) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			fprintf(stderr, "ERROR: Store '%s' not found in config\n",
			        g_opts.store_filter);
			return 1;
		}
	}

	/* Validate umesnaprepo binary supports required options */
	if (validate_umesnaprepo() != 0) {
		return 1;
	}

	/* Validate-only mode */
	if (g_opts.validate_only) {
		printf("Configuration is valid.\n");
		ume_config_print(stdout, &g_config);
		if (g_opts.umestored_extra_argc > 0) {
			int i;
			printf("  umestored args:");
			for (i = 0; i < g_opts.umestored_extra_argc; i++)
				printf(" %s", g_opts.umestored_extra_argv[i]);
			printf("\n");
		}
		return 0;
	}

	/* Operator confirmation */
	if (confirm_config() != 0) {
		printf("Aborted by operator.\n");
		return 0;
	}

	/* Open maintenance log */
	g_log_fp = fopen(g_opts.maint_log_path, "a");
	if (!g_log_fp) {
		fprintf(stderr, "WARNING: Cannot open maintenance log: %s\n",
		        g_opts.maint_log_path);
		/* Non-fatal: continue with stdout only */
	}

	/* Setup signal handlers */
	setup_signals();

	/* Check for stale or running umestored from a previous session */
#ifndef _WIN32
	if (check_existing_pid_file() != 0)
		return 1;
#endif

	/* Initialize keyword ring */
	keyword_ring_init(&g_keyword_ring);

	/* Initialize stats */
	memset(&g_stats, 0, sizeof(g_stats));

	LOG_INFO("=== %s v%s starting ===", PROG_NAME, VERSION);
	LOG_INFO("Config: %s", g_opts.xml_config_path);
	LOG_INFO("Schedule: %s", g_opts.schedule_str);
	LOG_INFO("Keyword: %s (N=%d, M=%d)",
	         g_opts.keyword_str, g_opts.keyword_threshold, g_opts.keyword_window);

	/* Start the child process */
	if (start_child() != 0) {
		LOG_ERROR("Failed to start umestored");
		if (g_log_fp) fclose(g_log_fp);
		return 1;
	}

	/* Open store log file for monitoring (if configured) */
	open_store_logfile();

	/* Enter main loop */
	main_loop();

	/* Shutdown */
	LOG_INFO("Shutting down...");
	stop_child();

	LOG_STATS("Final stats: initiated=%d completed=%d errors=%d failed=%d",
	          g_stats.initiated, g_stats.completed, g_stats.errors, g_stats.failed);
	LOG_INFO("=== %s exiting ===", PROG_NAME);

	if (g_logfile_fp) fclose(g_logfile_fp);
	if (g_log_fp) fclose(g_log_fp);

	return 0;
}
