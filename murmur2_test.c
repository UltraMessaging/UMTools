/* murmur2_test.c */

/* Copyright (c) 2019 Informatica Corporation  Permission is granted
 * to use or alter this software for any purpose, including commercial
 * applications.
 *
 * This source code example is provided by Informatica for educational
 * and evaluation purposes only.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES
 * OF NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 * PURPOSE. INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE. INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
 * BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL
 * OR INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 * TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED
 * OF THE LIKELIHOOD OF SUCH DAMAGES.
 */

/* murmur2_test.c - program to read a set of topic names and display the
 * resulting hash chain counts and lengths for the default UM resolver hash
 * function murmur2.
 *
 * See https://ultramessaging.github.io/currdoc/doc/Config/grpudpbasedresolveroperation.html#resolverstringhashfunctioncontext
 *
 * NOTE: This program was not written to be any kind of general test of the
 * murmur2 algorithm.  It is intended as a means to calculate and display
 * the UM resolver hash table chain lengths for a given set of topic names.
 *
 * BUILD:
 *   gcc -o murmur2_test murmur2_test.c
 * Note that the program is self-contained and does not link with UM.
 *
 * USAGE:
 *   murmur2_test topics.txt
 * Where topics.txt contains a list of topic names, one per line.
 *
 * SAMPLE OUTPUT:
 *   Reading topics from db_topics.txt...
 *   Analyzing topics...
 *   Hash results...
 *   ---Topic TOTAL: 1903 Hash Size: 131111 (in bytes: 524444)
 *   ---Hash 1 results: 1865
 *   ---Hash 2 results: 19
 *   ---Hash 3 results: 0
 *   ---Hash 4 results: 0
 *   ---Hash 5 results: 0
 *   ---Hash 6 results: 0
 *   ---Hash 7 results: 0
 *   ---Hash 8 results: 0
 *   ---Hash 9 results: 0
 *   ---Hash greater than 9 results: 0
 * Where "Hash 1 results: 1865" means that of the 1903 input topic names,
 * 1865 hash buckets had no collisions (chain length 1).  Only 19 buckets
 * had chain lengths of 2 (i.e. 2 topic names hashed to the bucket).
 * So 1865*1 + 19*2 = 1903 topics.
 */

/* On 5-Apr-2019, the following comment block was extracted from:
 * https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
 * --------------------------------------------------------------------
 * MurmurHash2 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
 * Note - This code makes a few assumptions about how your machine behaves -
 *
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */

/*
 * The murmur2 code contained herein has been slightly changed from the
 * original (e.g. seed is a hard-coded constant).
 */

#include <stdio.h>
#include <string.h>

unsigned long hash_topic_sym_murmur2(const char * key, size_t len)
{
	/* 'm' and 'r' are mixing constants generated offline.
	   They're not really 'magic', they just happen to work well. */

	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	unsigned int h;

	/* Mix 4 bytes at a time into the hash */

	const unsigned char * data = (const unsigned char *)key;

	/* Zero is not allowed */
	if (len == 0)
		len = strlen(key);

	/* Initialize the hash to a 'random' value */
	h = 0xdeadbeef ^ len;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	/* Handle the last few bytes of the input array */

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	/* Do a few final mixes of the hash to ensure the last few
	   bytes are well-incorporated. */

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return (unsigned long)h;
}

/* Some convenient prime numbers */
/* 131111 UM's default hash table size */
/* 311111 */
/* 611111 */
/* 588631 */
/* 169991 */
#define HASH_TABLE_SIZE 131111
int results[HASH_TABLE_SIZE];

#define MURMUR2_MAX_TOPICS 200000
char *topics[MURMUR2_MAX_TOPICS];

int main(int argc, char *argv[])
{
	char local_string[256];
	int loop;
	int max;
	unsigned int hash;
	unsigned int index;
	FILE *fp = NULL;

	if (argc != 2) {
		printf("ERROR: need 1 parameter which is the filename containing topics\n");
		return(1);
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		printf("ERROR: must supply valid filename (%s)\n", argv[1]);
		return(1);
	}

	printf("Reading topics from %s...\n", argv[1]);
	for (loop=0; loop<MURMUR2_MAX_TOPICS; loop++) {
		if (fgets(local_string, 256, fp) == NULL) break;
		topics[loop] = strdup(local_string);
		topics[loop][strlen(local_string)-1] = '\0'; /* Turn line feed into null (assume no \r) */
		/* printf("%d:(%s)\n", loop, topics[loop]); */
	}
	max = loop;

	printf("Analyzing topics...\n");
	memset(results, 0, sizeof(results));
	for (loop=0; loop<max; loop++) {
		hash = (unsigned int) (hash_topic_sym_murmur2(topics[loop], strlen(topics[loop])));
		index = hash % HASH_TABLE_SIZE;
		results[index]++;
		/* This is a nice output to see all the topics, the raw hash, and the index into the hash table. */
		/* printf("Topic(%s) Hash(%u) Index(%d)\n", topics[loop], hash, index); */
	}

	printf("Hash results...\n");
	printf("---Topic TOTAL: %d Hash Size: %d (in bytes: %d)\n", max, HASH_TABLE_SIZE, sizeof(results));

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 1) {
			max++;
		}
	}
	printf("---Hash 1 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 2) {
			max++;
		}
	}
	printf("---Hash 2 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 3) {
			max++;
		}
	}
	printf("---Hash 3 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 4) {
			max++;
		}
	}
	printf("---Hash 4 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 5) {
			max++;
		}
	}
	printf("---Hash 5 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 6) {
			max++;
		}
	}
	printf("---Hash 6 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 7) {
			max++;
		}
	}
	printf("---Hash 7 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 8) {
			max++;
		}
	}
	printf("---Hash 8 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] == 9) {
			max++;
		}
	}
	printf("---Hash 9 results: %d\n", max);

	max = 0;
	for (loop=0; loop<HASH_TABLE_SIZE; loop++) {
		if (results[loop] > 9) {
			max++;
		}
	}
	printf("---Hash greater than 9 results: %d\n", max);

}
