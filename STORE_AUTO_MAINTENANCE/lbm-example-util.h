/*
  All of the documentation and software included in this and any
  other Informatica Corporation Ultra Messaging Releases
  Copyright (C) Informatica Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted only as covered by the terms of a
  valid software license agreement with Informatica Corporation.

  Copyright (C) 2004-2021, Informatica Corporation. All Rights Reserved.

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
 All of the documentation and software included in this and any
 other Informatica Corporation Ultra Messaging Releases
 Copyright (C) Informatica Corporation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted only as covered by the terms of a
 valid software license agreement with Informatica Corporation.

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

#ifndef LBM_EXAMPLE_UTIL_H
#define LBM_EXAMPLE_UTIL_H

#include <time.h>
#ifdef _WIN32
	#include <sys/timeb.h>
#else
	#include <sys/time.h>
#endif

#define USECS_IN_SECOND 1000000
#define UMS_EXAMPLE_ERROR 1

int calc_rate(lbm_uint64_t *, char[], char[]);

/* Taken from ACE */
void normalize_tv(struct timeval *tv)
{
	if (tv->tv_usec >= USECS_IN_SECOND) {
			/* if more than 5 trips through loop required, then do divide and mod */
			if (tv->tv_usec >= (USECS_IN_SECOND*5)) {
				tv->tv_sec += (tv->tv_usec / USECS_IN_SECOND);
				tv->tv_usec = (tv->tv_usec % USECS_IN_SECOND);
			} else {
				do {
					tv->tv_sec++;
					tv->tv_usec -= USECS_IN_SECOND;
				} while (tv->tv_usec >= USECS_IN_SECOND);
			}
	} else if (tv->tv_usec <= -USECS_IN_SECOND) {
			/* if more than 5 trips through loop required, then do divide and mod */
			if (tv->tv_usec <= (-USECS_IN_SECOND*5)) {
				tv->tv_sec -= (tv->tv_usec / -USECS_IN_SECOND); /* neg / neg = pos so subtract */
				tv->tv_usec = (tv->tv_usec % -USECS_IN_SECOND); /* neg % neg = neg */
			} else {
				do {
					tv->tv_sec--;
					tv->tv_usec += USECS_IN_SECOND;
				} while (tv->tv_usec <= -USECS_IN_SECOND);
			}
	}
	if (tv->tv_sec >= 1 && tv->tv_usec < 0) {
		tv->tv_sec--;
		tv->tv_usec += USECS_IN_SECOND;
	} else if (tv->tv_sec < 0 && tv->tv_usec > 0) {
			tv->tv_sec++;
			tv->tv_usec -= USECS_IN_SECOND;
	}
}

/* Utility to return the current time of day */
void current_tv(struct timeval *tv)
{
#if defined(_WIN32) && !defined(WIN32_HIGHRES_TIME)
	struct _timeb tb;
	_ftime(&tb);
	tv->tv_sec = tb.time;
	tv->tv_usec = 1000 * tb.millitm;
#elif defined(_WIN32) && defined(WIN32_HIGHRES_TIME)
	LARGE_INTEGER ticks;
	static LARGE_INTEGER freq;
	static int first = 1;

	if (first) {
		QueryPerformanceFrequency(&freq);
		first = 0;
	}
	QueryPerformanceCounter(&ticks);
	tv->tv_sec = 0;
	tv->tv_usec = (1000000 * ticks.QuadPart / freq.QuadPart);
	normalize_tv(tv);
#else
	gettimeofday(tv,NULL);
#endif /* _WIN32 */
}

int parse_rate(char *arg,char *rm_protocol, lbm_uint64_t *rm_rate, lbm_uint64_t *rm_retrans)
{
	char rate[50], retr_rate[50], mult[2];
	int rc; 
	
	rc = sscanf(arg, "%[a-zA-Z]%50[^/]/%50s", rm_protocol, rate, retr_rate);
	if (rc == 3) {
		if (rm_protocol[1] != '\0') {	/* Invalid character after protocol */
			return UMS_EXAMPLE_ERROR;
		}
		switch (*rm_protocol)
		{
			case 'u': case 'U':
				*rm_protocol = 'U';
				break;
			case 'm': case 'M':
				*rm_protocol = 'M';
				break;
			default:				
				return UMS_EXAMPLE_ERROR;
		}	
	} else {
		*rm_protocol = 'M';		/* default protocol */
		rc = sscanf(arg, "%50[^/]/%50s", rate, retr_rate);
      if (rc != 2) {
      	return UMS_EXAMPLE_ERROR;
      }
	}
				
	/* Calculate transmisison rate */
	if (calc_rate(rm_rate, rate, mult) == UMS_EXAMPLE_ERROR) {
		return UMS_EXAMPLE_ERROR;
	}
	
	/* Calculate retransmission rate */
	if (calc_rate(rm_retrans, retr_rate, mult) == UMS_EXAMPLE_ERROR) {
		return UMS_EXAMPLE_ERROR;
	}
		
	return 0;
}

int calc_rate(lbm_uint64_t *rate, char val[], char mult[]) {
	int rc = sscanf(val, "%" SCNu64 "%2s", rate, mult);
	
	if (rc == 2){
		if (mult[1] != '\0') { /* Invalid character after mult */
			return UMS_EXAMPLE_ERROR;
		}
		switch (mult[0])
		{
			case 'k': case 'K':
				*rate *= (lbm_uint64_t)1000;
				break;
			case 'm': case 'M':
				*rate *= (lbm_uint64_t)1000000;
				break;
			case 'g': case 'G':
				*rate *= (lbm_uint64_t)1000000000;
				break;
			case '%':
				fprintf(stderr, "\n** ERROR - Please reference the updated usage. " 
								"Retransmission \n           rate no longer allows "
								"%% symbol as a shortcut.\n\n");	
				return UMS_EXAMPLE_ERROR;
			default:
				return UMS_EXAMPLE_ERROR;
		}		
	} else if (rc == 0) { 		/* Invalid character in front */
		return UMS_EXAMPLE_ERROR;
	} /* Else 1 implies a number without abbreviation */

	return 0;
}

#endif

