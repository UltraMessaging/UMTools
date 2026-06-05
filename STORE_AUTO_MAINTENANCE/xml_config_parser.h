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
 * xml_config_parser.h - Minimal XML parser for UME store configuration files.
 * Extracts daemon log path and per-store state/cache directories.
 */

#ifndef XML_CONFIG_PARSER_H
#define XML_CONFIG_PARSER_H

#define MAX_PATH_LEN 1024
#define MAX_STORE_NAME 256
#define MAX_STORES 64

typedef struct {
	char name[MAX_STORE_NAME];
	char cache_dir[MAX_PATH_LEN];
	char state_dir[MAX_PATH_LEN];
} store_info_t;

typedef struct {
	char log_path[MAX_PATH_LEN];      /* from <daemon><log> */
	int  log_configured;               /* 1 if <log> element found */
	store_info_t stores[MAX_STORES];
	int store_count;
} ume_config_t;

/*
 * Parse a UME store XML config file.
 * Returns 0 on success, -1 on error.
 * Error messages are written to stderr.
 */
int ume_config_parse(const char *xml_path, ume_config_t *config);

/*
 * Print parsed config summary to the given FILE stream.
 */
void ume_config_print(FILE *fp, const ume_config_t *config);

#endif /* XML_CONFIG_PARSER_H */
