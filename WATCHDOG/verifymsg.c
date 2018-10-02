/*
  Verifiable message routines for LBM example programs.

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

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char *iov_base;
	size_t iov_len;
} lbm_iovec_t;

#include "verifymsg.h"

#define VERIFIABLE_MAGIC_NUMBER "\x1b\x33\x56\xda"
#define VERIFIABLE_MAGIC_NUMBER_LEN 4
#define MINIMUM_VERIFIABLE_MSG_LEN VERIFIABLE_MAGIC_NUMBER_LEN + sizeof(unsigned short int) + 2

#define FORCE_ALIGNED_ACCESS 0
#if defined(sparc) || defined(__sparc) || defined(__sparc__) || defined(__ia64__)
	#undef FORCE_ALIGNED_ACCESS
	#define FORCE_ALIGNED_ACCESS 1
#endif

#if FORCE_ALIGNED_ACCESS
size_t AlignmentBufferSize = 0;
unsigned short int * AlignmentBuffer = NULL;
#endif

size_t
minimum_verifiable_msglen(void)
{
	return (MINIMUM_VERIFIABLE_MSG_LEN);
}

static int rand_seeded = 0;

/* Create random content message with checksum */

int
construct_verifiable_msg(char * data, size_t len)
{
	unsigned short int * datap = NULL;
	unsigned short int * cksum = (unsigned short int *) data;
	char * magicp = data + sizeof(unsigned short int);
	size_t len_left = len - sizeof(unsigned short int) - VERIFIABLE_MAGIC_NUMBER_LEN;

	if (!rand_seeded)
	{
		srand(time(NULL));
		rand_seeded = 1;
	}

	memset(data, 0, len);
	memcpy(magicp, VERIFIABLE_MAGIC_NUMBER, VERIFIABLE_MAGIC_NUMBER_LEN);
	datap = (unsigned short int *)(data + VERIFIABLE_MAGIC_NUMBER_LEN + sizeof(unsigned short int));

	while (len_left > sizeof(unsigned short int))
	{
		/* According to `man 3 random`, "... the low dozen bits generated
		   by rand go through a cyclic pattern." */
		*datap = (unsigned short int) ((rand() >> 12) & 0xff);
		datap++;
		len_left -= sizeof(unsigned short int);
	}

	*cksum = inet_cksum((unsigned short int *)data, len);
	return 0;
}

int
construct_verifiable_msgv(const lbm_iovec_t *iov, int count)
{
	unsigned short int * datap = NULL;
	unsigned short int * cksum = (unsigned short int *) iov[0].iov_base;
	char * magicp = iov[0].iov_base + sizeof(unsigned short int);
	size_t len_left = iov[0].iov_len - sizeof(unsigned short int) - VERIFIABLE_MAGIC_NUMBER_LEN;
	size_t total_len = 0;
	int i = 0;
	char *total_msg = NULL, *total_msgp = NULL;

	if (!rand_seeded)
	{
		srand(time(NULL));
		rand_seeded = 1;
	}
	for (i = 0; i < count; i++) {
		total_len += iov[i].iov_len;
	}
	total_msg = malloc(total_len);
	total_msgp = total_msg;

	memset(iov[0].iov_base, 0, iov[0].iov_len);
	memcpy(magicp, VERIFIABLE_MAGIC_NUMBER, VERIFIABLE_MAGIC_NUMBER_LEN);
	datap = (unsigned short int *)(iov[0].iov_base + VERIFIABLE_MAGIC_NUMBER_LEN + sizeof(unsigned short int));

	while (len_left > sizeof(unsigned short int))
	{
		/* According to `man 3 random`, "... the low dozen bits generated
		   by rand go through a cyclic pattern." */
		*datap = (unsigned short int) ((rand() >> 12) & 0xff);
		datap++;
		len_left -= sizeof(unsigned short int);
	}
	memcpy(total_msgp, iov[0].iov_base, iov[0].iov_len);
	total_msgp += iov[0].iov_len;
	for (i = 1; i < count; i++) {
		datap = (unsigned short int *)(iov[i].iov_base);
		len_left = iov[i].iov_len;
		memset(iov[i].iov_base, 0, iov[i].iov_len);
		while (len_left > sizeof(unsigned short int)) {
			/* According to `man 3 random`, "... the low dozen bits generated
			   by rand go through a cyclic pattern." */
			*datap = (unsigned short int) ((rand() >> 12) & 0xff);
			datap++;
			len_left -= sizeof(unsigned short int);
		}
		memcpy(total_msgp, iov[i].iov_base, iov[i].iov_len);
		total_msgp += iov[i].iov_len;
	}
	*cksum = inet_cksum((unsigned short int *)total_msg, total_len);
	free(total_msg);
	return 0;
}

/* Utility to check previously checksum'd buffer. Returns 1 if correct,
   0 if incorrect, and -1 if the message is not a verifiable message. */
int
verify_msg(const char * Data, size_t Length, int Verbose)
{
	unsigned short int calced_cksum = 0;
	unsigned short int * dataptr = NULL;

	if (Length < MINIMUM_VERIFIABLE_MSG_LEN)
	{
		return (-1);
	}
#if FORCE_ALIGNED_ACCESS
	if (AlignmentBufferSize < Length)
	{
		AlignmentBufferSize = Length;
		if (AlignmentBuffer != NULL)
		{
			free((void *) AlignmentBuffer);
		}
		AlignmentBuffer = (unsigned short *) malloc(AlignmentBufferSize);
	}
	memcpy((void *) AlignmentBuffer, Data, Length);
	dataptr = AlignmentBuffer;
#else
	dataptr = (unsigned short *) Data;
#endif

	/* nice thing about the inet_cksum is that it calculates to 0 when
	   the checksum is included. Makes checking it trivial. */
	calced_cksum = inet_cksum(dataptr, Length);
	if (Verbose)
	{
		printf("Message calculated cksum 0x%x\n", calced_cksum);
	}
	if (calced_cksum == 0)
	{
		return (1);			/* Success */
	}

	/* Checksum doesn't match. Make sure the magic number is there. */
	if (memcmp(Data + sizeof(unsigned short int), VERIFIABLE_MAGIC_NUMBER, VERIFIABLE_MAGIC_NUMBER_LEN) != 0)
	{
		return (-1);		/* Not verifiable */
	}
	return (0);				/* Failure */
}

/*
 * inet_cksum extracted from:
 *			P I N G . C
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *	Public Domain.  Distribution Unlimited.
 *
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */

unsigned short int
inet_cksum(unsigned short int *addr, size_t len)
{
	register int nleft = (int)len;
	register unsigned short int *w = addr;
	unsigned short int answer = 0;
	register unsigned int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while( nleft > 1 )  {
		sum += *w++;
		if (sum & 0x80000000)
			sum = (sum >> 16) + (sum & 0xffff);
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		*(unsigned char *) (&answer) = *(unsigned char *)w ;
		sum += answer;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	while (sum >> 16)
		sum = (sum >> 16) + (sum & 0xffff);  /* add hi 16 to low 16 */
	answer = ~sum;				     /* truncate to 16 bits */
	return (answer);
}

