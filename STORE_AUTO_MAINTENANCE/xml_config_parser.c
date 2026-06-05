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
 * xml_config_parser.c - Minimal XML parser for UME store configuration files.
 *
 * Parses only the elements needed for store auto-maintenance:
 *   <daemon><log>PATH</log></daemon>
 *   <stores><store name="NAME">
 *     <ume-attributes>
 *       <option type="store" name="disk-cache-directory" value="PATH"/>
 *       <option type="store" name="disk-state-directory" value="PATH"/>
 *     </ume-attributes>
 *   </store></stores>
 *
 * This is NOT a general-purpose XML parser. It relies on the well-defined
 * structure of the UME store config format.
 */

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xml_config_parser.h"

/* Read entire file into a malloc'd buffer. Caller must free.
 * Open in binary mode so the byte count from ftell() matches what fread()
 * returns. On Windows, text mode translates CRLF -> LF and the counts diverge,
 * causing a false-positive "Failed to read" error. */
static char *read_file(const char *path, long *out_len)
{
	FILE *fp = fopen(path, "rb");
	char *buf;
	long len;

	if (!fp) {
		fprintf(stderr, "ERROR: Cannot open config file: %s\n", path);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buf = (char *)malloc(len + 1);
	if (!buf) {
		fprintf(stderr, "ERROR: malloc failed for config file\n");
		fclose(fp);
		return NULL;
	}
	if (fread(buf, 1, len, fp) != (size_t)len) {
		fprintf(stderr, "ERROR: Failed to read config file: %s\n", path);
		free(buf);
		fclose(fp);
		return NULL;
	}
	buf[len] = '\0';
	if (out_len) *out_len = len;
	fclose(fp);
	return buf;
}

/* Skip whitespace, return pointer to next non-space char */
static const char *skip_ws(const char *p)
{
	while (*p && isspace((unsigned char)*p)) p++;
	return p;
}

/* Find next occurrence of tag like <tagname or </tagname, return pointer to '<' */
static const char *find_tag(const char *p, const char *tagname, int closing)
{
	char pattern[256];
	if (closing)
		snprintf(pattern, sizeof(pattern), "</%s", tagname);
	else
		snprintf(pattern, sizeof(pattern), "<%s", tagname);

	while ((p = strstr(p, pattern)) != NULL) {
		const char *after = p + strlen(pattern);
		/* Ensure this is actually a tag boundary (followed by space, >, /) */
		if (*after == '>' || *after == ' ' || *after == '\t' ||
		    *after == '\n' || *after == '\r' || *after == '/' ||
		    (closing && *after == '>')) {
			return p;
		}
		p = after;
	}
	return NULL;
}

/* Extract text content between > and </ for a simple element.
 * p should point to the '<' of the opening tag.
 * Returns pointer past the closing tag, or NULL on failure. */
static const char *extract_text_content(const char *p, const char *tagname,
                                        char *out, int out_size)
{
	const char *gt, *end;
	int len;
	char close_tag[256];

	/* Find the end of the opening tag */
	gt = strchr(p, '>');
	if (!gt) return NULL;
	gt++; /* past '>' */

	snprintf(close_tag, sizeof(close_tag), "</%s>", tagname);
	end = strstr(gt, close_tag);
	if (!end) return NULL;

	len = (int)(end - gt);
	if (len >= out_size) len = out_size - 1;

	/* Copy and trim whitespace */
	memcpy(out, gt, len);
	out[len] = '\0';

	/* Trim leading/trailing whitespace */
	while (len > 0 && isspace((unsigned char)out[len - 1])) {
		out[--len] = '\0';
	}
	{
		char *s = out;
		while (*s && isspace((unsigned char)*s)) s++;
		if (s != out) memmove(out, s, strlen(s) + 1);
	}

	return end + strlen(close_tag);
}

/* Extract an attribute value from a tag string.
 * tag_start points to '<', searches within until '>'.
 * Looks for attr_name="value" and copies value to out. */
static int extract_attr(const char *tag_start, const char *attr_name,
                        char *out, int out_size)
{
	const char *tag_end, *p, *q;
	char search[256];
	int len;

	tag_end = strchr(tag_start, '>');
	if (!tag_end) return -1;

	/* Try both quote styles: name="value" and name='value' */
	snprintf(search, sizeof(search), "%s=\"", attr_name);
	p = strstr(tag_start, search);
	if (p && p < tag_end) {
		p += strlen(search);
		q = strchr(p, '"');
		if (q && q < tag_end) {
			len = (int)(q - p);
			if (len >= out_size) len = out_size - 1;
			memcpy(out, p, len);
			out[len] = '\0';
			return 0;
		}
	}

	snprintf(search, sizeof(search), "%s='", attr_name);
	p = strstr(tag_start, search);
	if (p && p < tag_end) {
		p += strlen(search);
		q = strchr(p, '\'');
		if (q && q < tag_end) {
			len = (int)(q - p);
			if (len >= out_size) len = out_size - 1;
			memcpy(out, p, len);
			out[len] = '\0';
			return 0;
		}
	}

	return -1;
}

/* Skip XML comments <!-- ... --> */
static const char *skip_comments(const char *p)
{
	while (1) {
		p = skip_ws(p);
		if (strncmp(p, "<!--", 4) == 0) {
			const char *end = strstr(p + 4, "-->");
			if (end)
				p = end + 3;
			else
				break;
		} else {
			break;
		}
	}
	return p;
}

/* Parse <daemon> section for <log> element */
static int parse_daemon(const char *daemon_start, const char *daemon_end,
                        ume_config_t *config)
{
	const char *p = daemon_start;

	while (p < daemon_end) {
		p = skip_comments(p);
		const char *log_tag = find_tag(p, "log", 0);
		if (log_tag && log_tag < daemon_end) {
			if (extract_text_content(log_tag, "log",
			    config->log_path, MAX_PATH_LEN) != NULL) {
				config->log_configured = 1;
			}
			break;
		}
		break;
	}
	return 0;
}

/* Parse a single <store> element for disk-cache-directory and disk-state-directory */
static int parse_store(const char *store_start, const char *store_end,
                       store_info_t *si)
{
	const char *p = store_start;

	/* Extract store name attribute */
	extract_attr(store_start, "name", si->name, MAX_STORE_NAME);

	/* Scan for <option> tags within this store */
	while (p < store_end) {
		const char *opt = find_tag(p, "option", 0);
		if (!opt || opt >= store_end) break;

		char opt_type[64] = {0};
		char opt_name[128] = {0};
		char opt_value[MAX_PATH_LEN] = {0};

		extract_attr(opt, "type", opt_type, sizeof(opt_type));
		extract_attr(opt, "name", opt_name, sizeof(opt_name));
		extract_attr(opt, "value", opt_value, sizeof(opt_value));

		if (strcmp(opt_type, "store") == 0) {
			if (strcmp(opt_name, "disk-cache-directory") == 0) {
				strncpy(si->cache_dir, opt_value, MAX_PATH_LEN - 1);
				si->cache_dir[MAX_PATH_LEN - 1] = '\0';
			} else if (strcmp(opt_name, "disk-state-directory") == 0) {
				strncpy(si->state_dir, opt_value, MAX_PATH_LEN - 1);
				si->state_dir[MAX_PATH_LEN - 1] = '\0';
			}
		}

		/* Advance past this option tag */
		const char *gt = strchr(opt, '>');
		if (!gt) break;
		p = gt + 1;
	}

	return 0;
}

int ume_config_parse(const char *xml_path, ume_config_t *config)
{
	char *buf;
	long len;
	const char *p, *end;

	memset(config, 0, sizeof(*config));

	buf = read_file(xml_path, &len);
	if (!buf) return -1;

	/* Parse <daemon> section */
	p = find_tag(buf, "daemon", 0);
	if (p) {
		end = find_tag(p, "daemon", 1);
		if (end) {
			parse_daemon(p, end, config);
		}
	}

	/* Parse <stores> section — find each <store> element */
	p = find_tag(buf, "stores", 0);
	if (!p) {
		fprintf(stderr, "ERROR: No <stores> section found in %s\n", xml_path);
		free(buf);
		return -1;
	}

	end = find_tag(p, "stores", 1);
	if (!end) {
		fprintf(stderr, "ERROR: No closing </stores> tag in %s\n", xml_path);
		free(buf);
		return -1;
	}

	/* Iterate through <store> elements */
	{
		const char *sp = p;
		while (sp < end && config->store_count < MAX_STORES) {
			const char *store_start = find_tag(sp, "store", 0);
			const char *store_end;

			if (!store_start || store_start >= end) break;

			/* Make sure this is <store and not <stores */
			{
				const char *after = store_start + 6; /* strlen("<store") */
				if (*after != '>' && *after != ' ' && *after != '\t' &&
				    *after != '\n' && *after != '\r') {
					sp = after;
					continue;
				}
			}

			store_end = find_tag(store_start + 1, "store", 1);
			if (!store_end || store_end > end) break;

			parse_store(store_start, store_end,
			            &config->stores[config->store_count]);

			/* Only count stores that have at least a state directory */
			if (config->stores[config->store_count].state_dir[0] != '\0') {
				config->store_count++;
			} else {
				fprintf(stderr, "WARNING: Store '%s' has no disk-state-directory, skipping\n",
				        config->stores[config->store_count].name);
			}

			sp = store_end + 1;
		}
	}

	if (config->store_count == 0) {
		fprintf(stderr, "ERROR: No stores with disk-state-directory found in %s\n",
		        xml_path);
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

void ume_config_print(FILE *fp, const ume_config_t *config)
{
	int i;

	fprintf(fp, "  Log source:  %s\n",
	        config->log_configured ? config->log_path : "(stdout/stderr pipe)");
	fprintf(fp, "  Stores found: %d\n", config->store_count);

	for (i = 0; i < config->store_count; i++) {
		fprintf(fp, "    [%d] %-20s state=%s  cache=%s\n",
		        i + 1,
		        config->stores[i].name,
		        config->stores[i].state_dir,
		        config->stores[i].cache_dir[0] ? config->stores[i].cache_dir : "(none)");
	}
}
