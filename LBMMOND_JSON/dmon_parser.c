/*
Copyright (c) 2005-2017 Informatica Corporation  Permission is granted to licensees to use
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

NOTES:
This is a JNI implementation of parsing utilities in the tnwgdmon.c (for tnwgd) and umedmon.c (for umestored) sample applications
The parsed string is printed with the verbose option and returned to the java calling routine in JSON/XML format.
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
#include <lbm/lbm.h>
#include <lbm/lbmmon.h>
#include "monmodopts.h"
#include "replgetopt.h"
#include "umedmonmsgs.h"
#include "tnwgdmonmsgs.h"
#include "lbmmond_json.h"

/* #include "lbm-example-util.h" */

#define COND_SWAP16(_bs,_n) ((_bs)? byte_swap16(_n) : _n)
#define COND_SWAP32(_bs,_n) ((_bs)? byte_swap32(_n) : _n)
#define COND_SWAP64(_bs,_n) ((_bs)? byte_swap64(_n) : _n)

#ifdef  _WIN64
#define offsetof(s,m)   (size_t)( (ptrdiff_t)&(((s *)0)->m) )
#elif !defined(_AIX)
#define offsetof(s,m)   (size_t)&(((s *)0)->m)
#endif




jint  tnwg_dmon_msg_handler(JNIEnv *env, const char *msg_buffer, jlong msg_size, jboolean verbose_flag, char * aligned_msg_buffer,  char * charArrayOutputBuffer,int ArrayLength);
jint   ump_dmon_msg_handler(JNIEnv *env, const char *msg_buffer, jlong msg_size, jboolean verbose_flag, char * aligned_msg_buffer, char * charArrayOutputBuffer, int ArrayLength);


int lbm_snprintf(char *str, size_t sz, const char *format, ...)
{
        va_list argp;
        int result;

        va_start(argp,format);
        result = vsnprintf(str,sz,format,argp);
        va_end(argp);
        return result;
}

lbm_uint16_t byte_swap16(lbm_uint16_t n16) {
	lbm_uint16_t h16 = 0;
	int i = 0;
	for (; i<2; ++i) {
		h16 <<= 8;
		h16 |= (n16 & 0xff);
		n16 >>= 8;
	}
	return h16;
}

lbm_uint32_t byte_swap32(lbm_uint32_t n32) {
	lbm_uint32_t h32 = 0;
	int i = 0;
	for (; i<4; ++i) {
		h32 <<= 8;
		h32 |= (n32 & 0xff);
		n32 >>= 8;
	}
	return h32;
}

lbm_uint64_t byte_swap64(lbm_uint64_t n64) {
	lbm_uint64_t h64 = 0;
	int i = 0;
	for (; i<8; ++i) {
		h64 <<= 8;
		h64 |= (n64 & 0xff);
		n64 >>= 8;
	}
	return h64;
}


size_t format_timeval(struct timeval *tv, char *buf, size_t sz)
{
	size_t written = -1;
	time_t tt = tv->tv_sec;
	struct tm *gm = gmtime(&tt);
	if (gm) {
		written = (size_t)strftime(buf, sz, "%Y-%m-%d %H:%M:%S GMT", gm);
	}
	return written;
}

char *aligned_msg_buffer;
char *UMP_JSON_buffer;

/*
 * Class:     lbmmond
 * Method:    stat_parser
 * Signature: (Ljava/nio/ByteBuffer;JZC[C)Ljava/lang/Boolean;
 * */
JNIEXPORT jint JNICALL Java_lbmmond_1json_stat_1parser
  (JNIEnv *j_env, jobject j_obj, jobject j_buff, jlong j_msg_size, jboolean j_verbose, jchar j_type,  jcharArray carr)
{

        const char *msg_buffer;
	char status;
	int msg_size = j_msg_size;
	jint ret_status=0;
	char *aligned_msg_buffer = NULL;
	char *JSON_buffer = NULL;
	jchar *JSONCharArrayBuffer = NULL;
        jint ArrayLen;
	int i;


        msg_buffer = (*j_env)->GetDirectBufferAddress(j_env, j_buff);
	if ( msg_buffer == NULL ){
		printf("\nERROR! accessing direct byte buffer; Perhaps given object is not a direct java.nio.Buffer, or JNI access to direct buffers is not supported by this virtual machine. (requires JDK/JRE 1.4) ");
		return JNI_FALSE;
	}
	
	JSONCharArrayBuffer = (*j_env)->GetCharArrayElements(j_env, carr, NULL);
	if( JSONCharArrayBuffer == NULL ){
		printf("\nERROR! accessing character buffer");
		return JNI_FALSE;
	}
	ArrayLen = (*j_env)->GetArrayLength(j_env, carr);
	if(j_verbose == 1){
		printf("\nNOTICE: JSONCharArrayBuffer length = %i", ArrayLen);
	}

	aligned_msg_buffer = malloc(msg_size + 16);
	if( aligned_msg_buffer == NULL ){
		printf("\n ERROR! Could not allocate memory for aligned_msg_buffer");
		return JNI_FALSE;
	}
	JSON_buffer = malloc(ArrayLen);
	if( JSON_buffer == NULL ){
		printf("\n ERROR! Could not allocate memory for JSON_buffer");
		return JNI_FALSE;
	}
	JSON_buffer[0] = '\0';
	aligned_msg_buffer[0] = '\0';


	if( j_type == 1 ){
		ret_status = ump_dmon_msg_handler(j_env, msg_buffer, msg_size, j_verbose, aligned_msg_buffer, JSON_buffer, ArrayLen); 
	} else 
	if( j_type == 2 ){
		ret_status = tnwg_dmon_msg_handler(j_env, msg_buffer, msg_size, j_verbose, aligned_msg_buffer, JSON_buffer, ArrayLen);
	} else {
		 printf("\n ERROR! stat_type = %i not implemented ", j_type);
	}


	if( ret_status != JNI_FALSE ){
		/* Store the JSON string in the array provided */
		
		for (i=0; (i<ArrayLen) && (JSON_buffer[i] != '\0'); i++){
                	JSONCharArrayBuffer[i] = JSON_buffer[i];
        	}
        	JSONCharArrayBuffer[i] = '\0';

        	(*j_env)->ReleaseCharArrayElements(j_env, carr, JSONCharArrayBuffer, 0);
		free(aligned_msg_buffer);
		free(JSON_buffer);
		return (i);

	} else {

        	(*j_env)->ReleaseCharArrayElements(j_env, carr, JSONCharArrayBuffer, 0);
		free(aligned_msg_buffer);
		free(JSON_buffer);
		return JNI_FALSE;
	}

}

jint ump_dmon_msg_handler(JNIEnv *env, const char *msg_buffer, jlong msg_size, jboolean verbose_flag, char *aligned_msg_buffer, char *UMP_JSON_buffer, int ArrayLength)
{
	int msg_swap;			/* 1 means byte swap message */
	lbm_uint16_t msg_type;		/* swabbed message type */
	lbm_uint16_t msg_length;	/* swabbed message length */
	lbm_uint16_t msg_version;	/* swabbed message version */
	lbm_uint32_t msg_tv_sec;	/* swabbed message timeval seconds */
	lbm_uint32_t msg_tv_usec;	/* swabbed message timeval microseconds */
	char time_buff_sent[100];
	char time_buff_rcvd[100];
	struct timeval sent_tv;
	umestore_dmon_msg_hdr_t *ump_msg_hdr;

	char unknown_msg = 0;		
	time_t now = time(0);
	umestore_dmon_msg_hdr_t *msg_hdr;


	memcpy(aligned_msg_buffer, msg_buffer, msg_size);
	ump_msg_hdr = (umestore_dmon_msg_hdr_t *)aligned_msg_buffer;

	msg_hdr = (umestore_dmon_msg_hdr_t *)msg_buffer;
	strftime(time_buff_rcvd, 100, "%Y-%m-%d %H:%M:%S", localtime(&now));
	if( verbose_flag == 1){
		printf("%s Received ", time_buff_rcvd);
	}
	if (msg_size < UMESTORE_DMON_MSG_HDR_T_SZ) {
		if( verbose_flag == 1){
			printf("undersized UMP message: %d\n!", msg_size);
		}
		return JNI_FALSE;
	}
	if (msg_hdr->magic != LBM_UMESTORE_DMON_MAGIC && msg_hdr->magic != LBM_UMESTORE_DMON_ANTIMAGIC) {
		if( verbose_flag == 1){
			printf("UMP message with bad magic: 0x%x\n!", msg_hdr->magic);
		}
		return JNI_FALSE;
	}
	msg_swap = (msg_hdr->magic != LBM_UMESTORE_DMON_MAGIC);
	msg_type = COND_SWAP16(msg_swap, msg_hdr->type);
	msg_length = COND_SWAP16(msg_swap, msg_hdr->length);
	msg_version = COND_SWAP16(msg_swap, msg_hdr->version);
	msg_tv_sec = COND_SWAP32(msg_swap, msg_hdr->tv_sec);
	msg_tv_usec = COND_SWAP32(msg_swap, msg_hdr->tv_usec);
	sent_tv.tv_sec = msg_tv_sec;
	sent_tv.tv_usec = msg_tv_usec;
	if (format_timeval(&sent_tv, time_buff_sent, sizeof(time_buff_sent)) <= 0) {
		strcpy(time_buff_sent, "unknown");
	}

	switch (msg_type) {
	case LBM_UMESTORE_DMON_MPG_SMART_HEAP_STATS:
	{
		umestore_smart_heap_dmon_stat_msg_t *msg = (umestore_smart_heap_dmon_stat_msg_t *)aligned_msg_buffer;
		if (msg_length < UMESTORE_SMART_HEAP_DMON_STAT_MSG_T_MIN_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_SMART_HEAP_STATS message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_SMART_HEAP_STATS Version: %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                      Store daemon version: %s\n", msg->umestored_version_buffer);
				printf("                         SmartHeap version: %d.%d.%d\n", COND_SWAP16(msg_swap, msg->mem_major_version), COND_SWAP16(msg_swap, msg->mem_minor_version), COND_SWAP16(msg_swap, msg->mem_update_version));
				printf("                      Memory usage (bytes): 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->poolsize));
				printf("                   Active allocation count: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->poolcount));
				printf("                  Small block size (bytes): 0x%08X\n", COND_SWAP32(msg_swap, msg->smallBlockSize));
				printf("                         Page size (bytes): 0x%08X\n\n", COND_SWAP32(msg_swap, msg->pageSize));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_SMART_HEAP_STATS\":{"
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"umestored_version_buffer\":\"%s\","
                                 "\"mem_major_version\":%d,"
                                 "\"mem_minor_version\":%d,"
                                 "\"mem_update_version\":%d,"
                                 "\"poolsize\":\"0x%016"PRIx64"\","
                                 "\"poolcount\":\"0x%016"PRIx64"\","
                                 "\"smallBlockSize\":\"0x%08X\","
                                 "\"pageSize\":\"0x%08X\"}",
                                 
				msg_version, time_buff_sent,
				msg->umestored_version_buffer,
				COND_SWAP16(msg_swap, msg->mem_major_version), COND_SWAP16(msg_swap, msg->mem_minor_version), COND_SWAP16(msg_swap, msg->mem_update_version),
				COND_SWAP64(msg_swap, msg->poolsize),
				COND_SWAP64(msg_swap, msg->poolcount),
				COND_SWAP32(msg_swap, msg->smallBlockSize),
				COND_SWAP32(msg_swap, msg->pageSize));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_STORE_STATS:
	{
		umestore_store_dmon_stat_msg_t *msg = (umestore_store_dmon_stat_msg_t *)aligned_msg_buffer;
		if (msg_length < UMESTORE_STORE_DMON_STAT_MSG_T_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_STORE_STATS message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_STORE_STATS Version: %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("  UME retransmission request receive count: 0x%08X\n", COND_SWAP32(msg_swap, msg->ume_retx_req_rcv_count));
				printf(" UME retransmission request serviced count: 0x%08X\n", COND_SWAP32(msg_swap, msg->ume_retx_req_serviced_count));
				printf("     UME retransmission request drop count: 0x%08X\n", COND_SWAP32(msg_swap, msg->ume_retx_req_drop_count));
				printf("    UME retransmission statistics interval: 0x%08X\n", COND_SWAP32(msg_swap, msg->ume_retx_stat_interval));
				printf("  UME retransmission request total dropped: 0x%08X\n\n", COND_SWAP32(msg_swap, msg->ume_retx_req_total_dropped));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_STORE_STATS\":{"
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"store_idx\":\"0x%04X\","
                                 "\"ume_retx_req_rcv_count\":\"0x%08X\","
                                 "\"ume_retx_req_serviced_count\":\"0x%08X\","
                                 "\"ume_retx_req_drop_count\":\"0x%08X\","
                                 "\"ume_retx_stat_interval\":\"0x%08X\","
                                 "\"ume_retx_req_total_dropped\":\"0x%08X\"}",
                                 
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP32(msg_swap, msg->ume_retx_req_rcv_count),
				COND_SWAP32(msg_swap, msg->ume_retx_req_serviced_count),
				COND_SWAP32(msg_swap, msg->ume_retx_req_drop_count),
				COND_SWAP32(msg_swap, msg->ume_retx_stat_interval),
				COND_SWAP32(msg_swap, msg->ume_retx_req_total_dropped));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_REPO_STATS:
	{
		umestore_repo_dmon_stat_msg_t *msg = (umestore_repo_dmon_stat_msg_t *)aligned_msg_buffer;
		if (msg_length < UMESTORE_REPO_DMON_STAT_MSG_T_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_REPO_STATS message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_REPO_STATS Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                       Monitor topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->dmon_topic_idx));
				printf("                           Registration ID: 0x%08X\n", COND_SWAP32(msg_swap, msg->regid));
				printf("                                Repo flags: 0x%02X\n", msg->flags);
				printf("                          Message map size: 0x%08X\n", COND_SWAP32(msg_swap, msg->message_map_sz));
				printf("                               Memory size: 0x%08X\n", COND_SWAP32(msg_swap, msg->memory_sz));
				printf("                           RPP Memory size: 0x%08X\n", COND_SWAP32(msg_swap, msg->rpp_memory_sz));
				printf("                      Lead sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->lead_sqn));
				printf("                      Sync sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->sync_sqn));
				printf("             Sync complete sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->sync_complete_sqn));
				printf("                     Trail sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->trail_sqn));
				printf("              Memory trail sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->mem_trail_sqn));
				printf("                Contiguous sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->contig_sqn));
				printf("                  High ULB sequence number: 0x%08X\n", COND_SWAP32(msg_swap, msg->high_ulb_sqn));
				printf("                     Map intentional drops: 0x%08X\n", COND_SWAP32(msg_swap, msg->map_intentional_drops));
				printf("                        Unrecoverable loss: 0x%08X\n", COND_SWAP32(msg_swap, msg->uls));
				printf("                 Unrecoverable loss bursts: 0x%08X\n", COND_SWAP32(msg_swap, msg->ulbs));
				printf("                          Size limit drops: 0x%08X\n\n", COND_SWAP32(msg_swap, msg->sz_limit_drops));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_REPO_STATS\":{"
                                         "\"version\":%d,"
                                         "\"time_buff_sent\":\"%s\","
                                         "\"store_idx\":\"0x%04X\","
                                         "\"dmon_topic_idx\":\"0x%08X\","
                                         "\"regid\":\"0x%08X\","
                                         "\"flags\":\"0x%02X\","
                                         "\"message_map_sz\":\"0x%08X\","
                                         "\"memory_sz\":\"0x%08X\","
                                         "\"rpp_memory_sz\":\"0x%08X\","
                                         "\"lead_sqn\":\"0x%08X\","
                                         "\"sync_sqn\":\"0x%08X\","
                                         "\"sync_complete_sqn\":\"0x%08X\","
                                         "\"trail_sqn\":\"0x%08X\","
                                         "\"mem_trail_sqn\":\"0x%08X\","
                                         "\"contig_sqn\":\"0x%08X\","
                                         "\"high_ulb_sqn\":\"0x%08X\","
                                         "\"map_intentional_drops\":\"0x%08X\","
                                         "\"uls\":\"0x%08X\","
                                         "\"ulbs\":\"0x%08X\","
                                         "\"sz_limit_drops\":\"0x%08X\"}",

					msg_version, time_buff_sent,
					COND_SWAP16(msg_swap, msg->store_idx),
					COND_SWAP32(msg_swap, msg->dmon_topic_idx),
					COND_SWAP32(msg_swap, msg->regid),
					msg->flags,
					COND_SWAP32(msg_swap, msg->message_map_sz),
					COND_SWAP32(msg_swap, msg->memory_sz),
					COND_SWAP32(msg_swap, msg->rpp_memory_sz),
					COND_SWAP32(msg_swap, msg->lead_sqn),
					COND_SWAP32(msg_swap, msg->sync_sqn),
					COND_SWAP32(msg_swap, msg->sync_complete_sqn),
					COND_SWAP32(msg_swap, msg->trail_sqn),
					COND_SWAP32(msg_swap, msg->mem_trail_sqn),
					COND_SWAP32(msg_swap, msg->contig_sqn),
					COND_SWAP32(msg_swap, msg->high_ulb_sqn),
					COND_SWAP32(msg_swap, msg->map_intentional_drops),
					COND_SWAP32(msg_swap, msg->uls),
					COND_SWAP32(msg_swap, msg->ulbs),
					COND_SWAP32(msg_swap, msg->sz_limit_drops));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_DISK_STATS:
	{
		umestore_disk_dmon_stat_msg_t *msg = (umestore_disk_dmon_stat_msg_t *)aligned_msg_buffer;
		if (msg_length < UMESTORE_DISK_DMON_STAT_MSG_T_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_DISK_STATS message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_DISK_STATS Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                       Monitor topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->dmon_topic_idx));
				printf("                           Registration ID: 0x%08X\n", COND_SWAP32(msg_swap, msg->regid));
				printf("                                Max offset: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->max_offset));
				printf("                    Number of I/Os pending: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->num_ios_pending));
				printf("               Number of read I/Os pending: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->num_read_ios_pending));
				printf("                                    Offset: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->offset));
				printf("                              Start offset: 0x%016"PRIx64"\n\n", COND_SWAP64(msg_swap, msg->start_offset));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_DISK_STATS\":{"
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"store_idx\":\"0x%04X\","
                                 "\"dmon_topic_idx\":\"0x%08X\","
                                 "\"regid\":\"0x%08X\","
                                 "\"max_offset\":\"0x%016"PRIx64"\","
                                 "\"num_ios_pending\":\"0x%016"PRIx64"\","
                                 "\"num_read_ios_pending\":\"0x%016"PRIx64"\","
                                 "\"offset\":\"0x%016"PRIx64"\","
                                 "\"start_offset\":\"0x%016"PRIx64"\"}",
                                 
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP32(msg_swap, msg->dmon_topic_idx),
				COND_SWAP32(msg_swap, msg->regid),
				COND_SWAP64(msg_swap, msg->max_offset),
				COND_SWAP64(msg_swap, msg->num_ios_pending),
				COND_SWAP64(msg_swap, msg->num_read_ios_pending),
				COND_SWAP64(msg_swap, msg->offset),
				COND_SWAP64(msg_swap, msg->start_offset));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_RCV_STATS:
	{
		umestore_rcv_dmon_stat_msg_t *msg = (umestore_rcv_dmon_stat_msg_t *)aligned_msg_buffer;
		if (msg_length < UMESTORE_RCV_DMON_STAT_MSG_T_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_RCV_STATS message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_RCV_STATS Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                       Monitor topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->dmon_topic_idx));
				printf("                           Registration ID: 0x%08X\n", COND_SWAP32(msg_swap, msg->regid));
				printf("                                     Flags: 0x%04X\n", COND_SWAP16(msg_swap, msg->flags));
				printf("                  High ack sequence number: 0x%08X\n\n", COND_SWAP32(msg_swap, msg->high_ack_sqn));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_RCV_STATS\":{"
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"store_idx\":\"0x%04X\","
                                 "\"dmon_topic_idx\":\"0x%08X\","
                                 "\"regid\":\"0x%08X\","
                                 "\"flags\":\"0x%04X\","
                                 "\"high_ack_sqn\":\"0x%08X\"}",
                                 
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP32(msg_swap, msg->dmon_topic_idx),
				COND_SWAP32(msg_swap, msg->regid),
				COND_SWAP16(msg_swap, msg->flags),
				COND_SWAP32(msg_swap, msg->high_ack_sqn));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_STORE_CONFIG:
	{
		umestore_store_dmon_config_msg_t *msg = (umestore_store_dmon_config_msg_t *)aligned_msg_buffer;
		char *msg_p = (char *)msg;
		struct in_addr in;
		if (msg_length < UMESTORE_STORE_DMON_CONFIG_MSG_T_MIN_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_STORE_CONFIG message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			in.s_addr = msg->store_iface;
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_STORE_CONFIG Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                   Store IP address / port: %s / %d\n", inet_ntoa(in), COND_SWAP16(msg_swap, msg->store_port));
				printf("                                Store name: %s\n", msg_p + COND_SWAP16(msg_swap, msg->store_name_offset));
				printf("                      Disk cache directory: %s\n", msg_p + COND_SWAP16(msg_swap, msg->disk_cache_dir_offset));
				printf("                      Disk state directory: %s\n", msg_p + COND_SWAP16(msg_swap, msg->disk_state_dir_offset)); 
				printf("      Store retransmission processing rate: 0x%08X\n\n", COND_SWAP32(msg_swap, msg->store_max_retransmission_processing_rate));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_STORE_CONFIG\":{"
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"store_idx\":\"0x%04X\","
                                 "\"store_iface\":\"%s\","
                                 "\"store_port\":%d,"
                                 "\"store_name_offset\":\"%s\","
                                 "\"disk_cache_dir_offset\":\"%s\","
                                 "\"disk_state_dir_offset\":\"%s\","
                                 "\"store_max_retransmission_processing_rate\":\"0x%08X\"}",
                                
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				inet_ntoa(in), COND_SWAP16(msg_swap, msg->store_port),
				msg_p + COND_SWAP16(msg_swap, msg->store_name_offset),
				msg_p + COND_SWAP16(msg_swap, msg->disk_cache_dir_offset),
				msg_p + COND_SWAP16(msg_swap, msg->disk_state_dir_offset),
				COND_SWAP32(msg_swap, msg->store_max_retransmission_processing_rate));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_STORE_PATTERN_CONFIG:
	{
		umestore_store_pattern_dmon_config_msg_t *msg = (umestore_store_pattern_dmon_config_msg_t *)aligned_msg_buffer;
		char buf[256];
		char *msg_p = (char *)msg->pattern_buffer;
		int buf_idx, msg_cnt;
		if (msg_length < UMESTORE_STORE_PATTERN_DMON_CONFIG_MSG_T_MIN_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_STORE_PATTERN_CONFIG message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			msg_cnt = msg_length - offsetof(umestore_store_pattern_dmon_config_msg_t, pattern_buffer);
			for (buf_idx = 0; buf_idx < msg_cnt; buf_idx++) {
				buf[buf_idx] = msg_p[buf_idx];
			}
			buf[buf_idx] = 0;
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_STORE_PATTERN_CONFIG Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                              Pattern type: 0x%04X\n", COND_SWAP16(msg_swap, msg->type));
				printf("                            Pattern string: %s\n\n", buf);
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_STORE_PATTERN_CONFIG\":{"
                                "\"version\":%d,\"time_buff_sent\":\"%s\",\"store_idx\":\"0x%04X\",\"type\":\"0x%04X\",\"pattern_buffer\":\"%s\"}",
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP16(msg_swap, msg->type),
				buf);
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG:
	{
		umestore_topic_dmon_config_msg_t *msg = (umestore_topic_dmon_config_msg_t *)aligned_msg_buffer;
		char buf[256];
		char *msg_p = (char *)msg->topic_name;
		int buf_idx, msg_cnt;
		if (msg_length < UMESTORE_TOPIC_DMON_CONFIG_MSG_T_MIN_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			msg_cnt = msg_length - offsetof(umestore_topic_dmon_config_msg_t, topic_name);
			for (buf_idx = 0; buf_idx < msg_cnt; buf_idx++) {
				buf[buf_idx] = msg_p[buf_idx];
			}
			buf[buf_idx] = 0;
			if( verbose_flag == 1){ 
				printf("LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                       Monitor topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->dmon_topic_idx));
				printf("                                Topic name: %s\n\n", buf);
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG\":{"
                                "\"version\":%d,\"time_buff_sent\":\"%s\",\"store_idx\":\"0x%04X\",\"dmon_topic_idx\":\"0x%08X\",\"topic_name\":\"%s\"}",
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP32(msg_swap, msg->dmon_topic_idx),
				buf);
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_REPO_CONFIG:
	{
		umestore_repo_dmon_config_msg_t *msg = (umestore_repo_dmon_config_msg_t *)aligned_msg_buffer;
		if (msg_length < UMESTORE_REPO_DMON_CONFIG_MSG_T_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_REPO_CONFIG message: %d\n", msg_size);
			}
			unknown_msg = 1;
		}
		else {
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_REPO_CONFIG Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                       Monitor topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->dmon_topic_idx));
				printf("                           Registration ID: 0x%08X\n", COND_SWAP32(msg_swap, msg->regid));
				printf("                                Session ID: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->sid));
				printf("                                Store type: 0x%02X\n", msg->type);
				printf("                            Size threshold: 0x%08X\n", COND_SWAP32(msg_swap, msg->sz_threshold));
				printf("                                Size limit: 0x%08X\n", COND_SWAP32(msg_swap, msg->sz_limit));
				printf("                             Age threshold: 0x%08X\n", COND_SWAP32(msg_swap, msg->age_threshold));
				printf("                     Disk max write AIOCBS: 0x%08X\n", COND_SWAP32(msg_swap, msg->disk_max_write_aiocbs));
				printf("                      Disk max read AIOCBS: 0x%08X\n", COND_SWAP32(msg_swap, msg->disk_max_read_aiocbs));
				printf("                           Disk size limit: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->disk_sz_limit));
				printf("                    DISK AIO buffer length: 0x%08X\n", COND_SWAP32(msg_swap, msg->disk_aio_buffer_len));
				printf("                    Allow ack on reception: 0x%02X\n", msg->allow_ack_on_reception);
				printf("                               Write delay: 0x%08X\n", COND_SWAP32(msg_swap, msg->write_delay));
				printf("                  Source flight size bytes: 0x%016"PRIx64"\n\n", COND_SWAP64(msg_swap, msg->src_flightsz_bytes));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_REPO_CONFIG\":{"
                                "\"version\":%d,"
                                "\"time_buff_sent\":\"%s\","
                                "\"store_idx\":\"0x%04X\","
                                "\"dmon_topic_idx\":\"0x%08X\","
                                "\"regid\":\"0x%08X\","
                                "\"sid\":\"0x%016"PRIx64"\","
                                "\"type\":\"0x%02X\","
                                "\"sz_threshold\":\"0x%08X\","
                                "\"sz_limit\":\"0x%08X\","
                                "\"age_threshold\":\"0x%08X\","
                                "\"disk_max_write_aiocbs\":\"0x%08X\","
                                "\"disk_max_read_aiocbs\":\"0x%08X\","
                                "\"disk_sz_limit\":\"0x%016"PRIx64"\","
                                "\"disk_aio_buffer_len\":\"0x%08X\","
                                "\"allow_ack_on_reception\":\"0x%02X\","
                                "\"write_delay\":\"0x%08X\","
                                "\"src_flightsz_bytes\":\"0x%016"PRIx64"\"}",
                                
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP32(msg_swap, msg->dmon_topic_idx),
				COND_SWAP32(msg_swap, msg->regid),
				COND_SWAP64(msg_swap, msg->sid),
				msg->type,
				COND_SWAP32(msg_swap, msg->sz_threshold),
				COND_SWAP32(msg_swap, msg->sz_limit),
				COND_SWAP32(msg_swap, msg->age_threshold),
				COND_SWAP32(msg_swap, msg->disk_max_write_aiocbs),
				COND_SWAP32(msg_swap, msg->disk_max_read_aiocbs),
				COND_SWAP64(msg_swap, msg->disk_sz_limit),
				COND_SWAP32(msg_swap, msg->disk_aio_buffer_len),
				msg->allow_ack_on_reception,
				COND_SWAP32(msg_swap, msg->write_delay),
				COND_SWAP64(msg_swap, msg->src_flightsz_bytes));
		}
		break;
	}
	case LBM_UMESTORE_DMON_MPG_RCV_CONFIG:
	{
		umestore_rcv_dmon_config_msg_t *msg = (umestore_rcv_dmon_config_msg_t *)aligned_msg_buffer;
		struct in_addr in;
		if (msg_length < UMESTORE_RCV_DMON_CONFIG_MSG_T_SZ) {
			if( verbose_flag == 1){
				printf("undersized LBM_UMESTORE_DMON_MPG_RCV_CONFIG message: %d\n", msg_size);
			}
			unknown_msg = 1;
		} else {
			in.s_addr = msg->sin_addr;
			if( verbose_flag == 1){
				printf("LBM_UMESTORE_DMON_MPG_RCV_CONFIG Version %d, Sent: %s\n", msg_version, time_buff_sent);
				printf("                               Store index: 0x%04X\n", COND_SWAP16(msg_swap, msg->store_idx));
				printf("                       Monitor topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->dmon_topic_idx));
				printf("                           Registration ID: 0x%08X\n", COND_SWAP32(msg_swap, msg->regid));
				printf("                                Session ID: 0x%016"PRIx64"\n", COND_SWAP64(msg_swap, msg->sid));
				printf("                                 Domain ID: 0x%08X\n", COND_SWAP32(msg_swap, msg->domain_id));
				printf("                           Transport index: 0x%08X\n", COND_SWAP32(msg_swap, msg->transport_idx));
				printf("                               Topic index: 0x%08X\n", COND_SWAP32(msg_swap, msg->topic_idx));
				printf("                         IP address / port: %s / %d\n\n", inet_ntoa(in), COND_SWAP16(msg_swap, msg->sin_port));
			}
			lbm_snprintf(UMP_JSON_buffer, ArrayLength,"\"LBM_UMESTORE_DMON_MPG_RCV_CONFIG\":{"
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"store_idx\":\"0x%04X\","
                                 "\"dmon_topic_idx\":\"0x%08X\","
                                 "\"regid\":\"0x%08X\","
                                 "\"sid\":\"0x%016"PRIx64"\","
                                 "\"domain_id\":\"0x%08X\","
                                 "\"transport_idx\":\"0x%08X\","
                                 "\"topic_idx\":\"0x%08X\","
                                 "\"sin_addr\":\"%s\","
                                 "\"sin_port\":%d}",
				msg_version, time_buff_sent,
				COND_SWAP16(msg_swap, msg->store_idx),
				COND_SWAP32(msg_swap, msg->dmon_topic_idx),
				COND_SWAP32(msg_swap, msg->regid),
				COND_SWAP64(msg_swap, msg->sid),
				COND_SWAP32(msg_swap, msg->domain_id),
				COND_SWAP32(msg_swap, msg->transport_idx),
				COND_SWAP32(msg_swap, msg->topic_idx),
				inet_ntoa(in), COND_SWAP16(msg_swap, msg->sin_port));
		}
		break;
	}
	default:
		if( verbose_flag == 1){
			printf("unknown message type 0x%x\n", msg_hdr->type);
		}
		unknown_msg = 1;
		break;
	}

	if( unknown_msg == 1 ){
		return JNI_FALSE;
	} else {
		return JNI_TRUE;
	}

} /* ump_dmon_msg_handler() */


jint tnwg_dmon_msg_handler(JNIEnv *env, const char *msg_buffer, jlong msg_size, jboolean verbose_flag, char *aligned_msg_buffer, char *TNWG_JSON_buffer, int ArrayLength)
{
	int msg_swap;			/* 1 means byte swap message */
	lbm_uint16_t msg_type;		/* swabbed message type */
	lbm_uint16_t msg_length;	/* swabbed message length */
	lbm_uint16_t msg_version;	/* swabbed message version */
	lbm_uint32_t msg_tv_sec;	/* swabbed message timeval seconds */
	lbm_uint32_t msg_tv_usec;	/* swabbed message timeval microseconds */
	char time_buff_sent[100];
	char time_buff_rcvd[100];
	struct timeval sent_tv;
	time_t now = time(0);
	tnwg_dstat_msg_hdr_t *dro_msg_hdr;
	char unknown_msg = 0;		


	if( TNWG_JSON_buffer == NULL || aligned_msg_buffer == NULL ){
		return JNI_FALSE;
	} else {
		TNWG_JSON_buffer[0] = '\0';
		aligned_msg_buffer[0] = '\0';
	}

	memcpy(aligned_msg_buffer, msg_buffer, msg_size);
	dro_msg_hdr = (tnwg_dstat_msg_hdr_t *)aligned_msg_buffer;
	if (dro_msg_hdr->magic != LBM_TNWG_DAEMON_MAGIC && dro_msg_hdr->magic != LBM_TNWG_DAEMON_ANTIMAGIC) {	
		printf("DRO stat message with bad magic: 0x%x\n!", dro_msg_hdr->magic);
		return JNI_FALSE;
	}
	strftime(time_buff_rcvd, 100, "%Y-%m-%d %H:%M:%S", localtime(&now));
	if( verbose_flag == 1){
		printf("\n%s Received ", time_buff_rcvd);
	}
	if (msg_size < sizeof(tnwg_dstat_msg_hdr_t)) {
		printf("DRO undersized stat message: %d\n!", msg_size);
		return JNI_FALSE;
	}
	msg_swap = (dro_msg_hdr->magic != LBM_TNWG_DAEMON_MAGIC);
	msg_type = COND_SWAP16(msg_swap, dro_msg_hdr->type);
	msg_length = COND_SWAP16(msg_swap, dro_msg_hdr->length);
	msg_version = COND_SWAP16(msg_swap, dro_msg_hdr->version);
	msg_tv_sec = COND_SWAP32(msg_swap, dro_msg_hdr->tv_sec);
	msg_tv_usec = COND_SWAP32(msg_swap, dro_msg_hdr->tv_usec);
	sent_tv.tv_sec = msg_tv_sec;
	sent_tv.tv_usec = msg_tv_usec;
	if (format_timeval(&sent_tv, time_buff_sent, sizeof(time_buff_sent)) <= 0) {
		strcpy(time_buff_sent, "unknown");
	}


	switch (msg_type) {
	case TNWG_DSTATTYPE_MALLINFO:
	{
		tnwg_dstat_mallinfo_msg_t *mallinfo_msg;
		tnwg_mallinfo_stat_grp_record_t *record;

		mallinfo_msg = (tnwg_dstat_mallinfo_msg_t *)aligned_msg_buffer;

		record = &mallinfo_msg->record;
		if (msg_length < sizeof(tnwg_dstat_mallinfo_msg_t)) {
			if( verbose_flag == 1){
				printf("undersized TNWG_DSTATTYPE_MALLINFO message: %d\n", msg_length);
			}
		} else {
			if( verbose_flag == 1){
				printf("\n==============Malloc Info (Version: %d)==============\n%s Sent\n", msg_version, time_buff_sent);
				printf("         arena: %d\n", COND_SWAP32(msg_swap, record->arena));
				printf("       ordblks: %d\n", COND_SWAP32(msg_swap, record->ordblks));
				printf("         hblks: %d\n", COND_SWAP32(msg_swap, record->hblks));
				printf("        hblkhd: %d\n", COND_SWAP32(msg_swap, record->hblkhd));
				printf("      uordblks: %d\n", COND_SWAP32(msg_swap, record->uordblks));
				printf("      fordblks: %d\n\n", COND_SWAP32(msg_swap, record->fordblks));
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"\"TNWG_DSTATTYPE_MALLINFO\":{"
						"\"version\":%d,"
                                                "\"time_buff_sent\":\"%s\","
                                                "\"arena\":%d,"
                                                "\"ordblks\":%d,"
                                                "\"hblks\":%d,"
                                                "\"hblkhd\":%d,"
                                                "\"uordblks\":%d,"
                                                "\"fordblks\":%d}",

						msg_version, time_buff_sent,
						COND_SWAP32(msg_swap, record->arena), 
						COND_SWAP32(msg_swap, record->ordblks),
						COND_SWAP32(msg_swap, record->hblks),
						COND_SWAP32(msg_swap, record->hblkhd),
						COND_SWAP32(msg_swap, record->uordblks),
						COND_SWAP32(msg_swap, record->fordblks));
		}
		break;
	}

	case TNWG_DSTATTYPE_GATEWAYCFG:
		{
			/* This returns an XML configuration file, so don't try to format for JSON */
			char * cfgstart;
			cfgstart = (char *) aligned_msg_buffer+sizeof(tnwg_dstat_msg_hdr_t);
			if( verbose_flag == 1){
				printf("\n======TNWG_DSTATTYPE_GATEWAYCFG Version: %d======\n%s Sent\n", msg_version, time_buff_sent);
				printf("%s\n\n", cfgstart);
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"%s",cfgstart);
		}
		break;
	case TNWG_DSTATTYPE_PORTCFG:
		{
			tnwg_pcfg_stat_grp_msg_t *pcfg_msg_p;
			int num_options, numpads;
			int i,j;
			int max_str_sz;
			char *dptr;
			char *dptr2;
			char *temp_char_buff2;

			pcfg_msg_p = (tnwg_pcfg_stat_grp_msg_t *) aligned_msg_buffer;
			dptr = &pcfg_msg_p->data;
			
			temp_char_buff2 = malloc(1024);

			num_options = COND_SWAP32(msg_swap, pcfg_msg_p->rechdr.num_options);
			if( verbose_flag == 1){
				printf("\n============%s Configuration(Version: %d)============\n%s Sent\n", pcfg_msg_p->rechdr.portal_name, msg_version, time_buff_sent);
				printf("=====================%s=====================\n", pcfg_msg_p->rechdr.attr_type);

				for(i = 0; i < num_options; i++){
					numpads = (strlen((const char*) dptr) > 50) ? 0 : (50 - strlen((const char*) dptr));
					for(j=0;j<numpads;j++) printf(" ");
					printf("%s", dptr);
					printf(": ");
					dptr += strlen((const char*) dptr) + 1;
					printf("%s\n", dptr);
					dptr += strlen((const char*) dptr) + 1;
				}
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"\"TNWG_DSTATTYPE_PORTCFG\":{\"portal_name\":\"%s\",\"version\":%d,\"time_buff_sent\":\"%s\",\"%s\":{",
				pcfg_msg_p->rechdr.portal_name, msg_version, time_buff_sent, pcfg_msg_p->rechdr.attr_type);
			max_str_sz = ArrayLength - strlen(TNWG_JSON_buffer) - 10;
			for(dptr = &pcfg_msg_p->data, i = 0; i < num_options; i++){
				dptr2 = dptr + strlen((const char*) dptr) + 1;
				if (i == 0){
					lbm_snprintf(temp_char_buff2, max_str_sz, "\"%s\":\"%s\"", dptr, dptr2);
				} else {
					lbm_snprintf(temp_char_buff2, max_str_sz, ",\"%s\":\"%s\"", dptr, dptr2);
				}
				j =  strlen(temp_char_buff2);
				max_str_sz -= j;
				if( max_str_sz > 0 && j > 0){
					strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);
					dptr = dptr2 + strlen((const char*) dptr2) + 1;
				} else {
					printf("\n WARNING! TNWG_DSTATTYPE_PORTCFG: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
					max_str_sz = 0;
					break;
				}
			}
			strncat(TNWG_JSON_buffer,"}}", max_str_sz);
			free(temp_char_buff2);
		}
		break;
	case TNWG_DSTATTYPE_RM_LOCAL:
		{
			tnwg_rm_stat_grp_msg_t * msg;
			tnwg_rm_stat_grp_local_info_t *p;
			msg = (tnwg_rm_stat_grp_msg_t *) aligned_msg_buffer;
			p = &msg->msgtype.local;
			if( verbose_flag == 1){
				printf("\n=================Gateway Topology Info(Version: %d)=================\n%s Sent\n", msg_version, time_buff_sent);
				printf("    Local Gateway Name: %s\n", p->gateway_name);
				printf("    Local Gateway ID: %"PRIu64"\n", COND_SWAP64(msg_swap, p->gateway_id));
				printf("    Self Version: %d\n", COND_SWAP16(msg_swap, p->self_version));
				printf("    Topology Signature: 0x%08x\n", COND_SWAP32(msg_swap, p->topology_signature));
				printf("    Last recalc duration: %u.%06u seconds\n", COND_SWAP32(msg_swap, p->recalc_duration_sec), COND_SWAP32(msg_swap, p->recalc_duration_usec));
				printf("    Graph Version: %d\n", COND_SWAP32(msg_swap, p->graph_version));
				printf("    Gateway Count: %d\n", COND_SWAP16(msg_swap, p->gateway_count));
				printf("    Topic Resolution Domain Count: %d\n", COND_SWAP16(msg_swap, p->trd_count));
			}
                        lbm_snprintf(TNWG_JSON_buffer,ArrayLength,"\"TNWG_DSTATTYPE_RM_LOCAL\":{"
                                     "\"version\":%d,"
                                     "\"time_buff_sent\":\"%s\","
                                     "\"gateway_name\":\"%s\","
                                     "\"gateway_id\":\"%"PRIu64"\","
                                     "\"self_version\":%d,"
                                     "\"topology_signature\":\"0x%08x\","
                                     "\"recalc_duration_sec\":%u,"
                                     "\"recalc_duration_usec\":%u,"
                                     "\"graph_version\":%d,"
                                     "\"gateway_count\":%d,"
                                     "\"trd_count\":%d}",

				msg_version, time_buff_sent,
				p->gateway_name,
				COND_SWAP64(msg_swap, p->gateway_id),
				COND_SWAP16(msg_swap, p->self_version),
				COND_SWAP32(msg_swap, p->topology_signature),
				COND_SWAP32(msg_swap, p->recalc_duration_sec), COND_SWAP32(msg_swap, p->recalc_duration_usec),
				COND_SWAP32(msg_swap, p->graph_version),
				COND_SWAP16(msg_swap, p->gateway_count),
				COND_SWAP16(msg_swap, p->trd_count)
				);
		}
		break;
	case TNWG_DSTATTYPE_RM_PORTAL:
		{
			tnwg_rm_stat_grp_msg_t * msg;
			tnwg_rm_stat_grp_portal_info_t * p;
			uint32_t ptype;
			uint32_t pindex;
			msg = (tnwg_rm_stat_grp_msg_t *) aligned_msg_buffer;
			p = &msg->msgtype.portal;

			pindex = COND_SWAP32(msg_swap, p->index);
			ptype = COND_SWAP32(msg_swap, p->type);

			if( verbose_flag == 1){
				printf("\n....................Portal %d (%s)(Version: %d)....................\n%s Sent\n", pindex, (ptype == TNWG_DSTAT_Portal_Type_Peer) ? "Peer" : "Endpoint",msg_version,time_buff_sent);
				printf("    Portal name: %s\n", p->portal_name);
				printf("    Adjacent ID: %"PRIu64"\n", COND_SWAP64(msg_swap, p->node_or_adjacent_id));
				printf("    Cost: %d\n", COND_SWAP32(msg_swap, p->ingress_cost));
				printf("    Last interest recalc duration: %d.%06u seconds\n", COND_SWAP32(msg_swap, p->recalc_duration_sec), COND_SWAP32(msg_swap, p->recalc_duration_usec));
				printf("    Last proxy receiver recalc duration: %d.%06u seconds\n", COND_SWAP32(msg_swap, p->proxy_rec_recalc_duration_sec), COND_SWAP32(msg_swap, p->proxy_rec_recalc_duration_usec));
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"\"TNWG_DSTATTYPE_RM_PORTAL\":{"
				 "\"pindex\":%d,"
                                 "\"ptype\":\"%s\","
                                 "\"version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"portal_name\":\"%s\","
                                 "\"node_or_adjacent_id\":\"%"PRIu64"\","
                                 "\"ingress_cost\":%d,"
                                 "\"recalc_duration_sec\":%d,"
                                 "\"recalc_duration_usec\":%u,"
                                 "\"proxy_rec_recalc_duration_sec\":%d,"
                                 "\"proxy_rec_recalc_duration_usec\":%u}",

				pindex, (ptype == TNWG_DSTAT_Portal_Type_Peer) ? "Peer" : "Endpoint",msg_version,time_buff_sent,
				p->portal_name,
				COND_SWAP64(msg_swap, p->node_or_adjacent_id),
				COND_SWAP32(msg_swap, p->ingress_cost),
				COND_SWAP32(msg_swap, p->recalc_duration_sec), COND_SWAP32(msg_swap, p->recalc_duration_usec),
				COND_SWAP32(msg_swap, p->proxy_rec_recalc_duration_sec), COND_SWAP32(msg_swap, p->proxy_rec_recalc_duration_usec));
		}
		break;
	case TNWG_DSTATTYPE_RM_OTHERGW:
		{
			tnwg_rm_stat_grp_msg_t * msg;
			tnwg_rm_stat_grp_othergw_info_t *p;
			msg = (tnwg_rm_stat_grp_msg_t *) aligned_msg_buffer;
			p = &msg->msgtype.othergw;

			if( verbose_flag == 1){
				printf("\n................Other Gateway(Version: %d)...............\n%s Sent\n", msg_version, time_buff_sent);
				printf("    Gateway Name: %s\n", p->gateway_name);
				printf("    Gateway ID: %"PRIu64"\n", COND_SWAP64(msg_swap, p->gateway_id));
				printf("    Version: %d\n", COND_SWAP16(msg_swap, p->version));
				printf("    Topology Signature: 0x%08x\n", COND_SWAP32(msg_swap, p->topology));
				printf("    Last Activity %d.%06u seconds ago\n", COND_SWAP32(msg_swap, p->last_activity_sec), COND_SWAP32(msg_swap, p->last_activity_usec));
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"\"TNWG_DSTATTYPE_RM_OTHERGW\":{"
				 "\"msg_version\":%d,"
                                 "\"time_buff_sent\":\"%s\","
                                 "\"gateway_name\":\"%s\","
                                 "\"gateway_id\":\"%"PRIu64"\","
                                 "\"version\":%d,"
                                 "\"topology\":\"0x%08x\","
                                 "\"last_activity_sec\":%d,"
                                 "\"last_activity_usec\":%u}",
                                 
				msg_version, time_buff_sent,
				p->gateway_name,
				COND_SWAP64(msg_swap, p->gateway_id),
				COND_SWAP16(msg_swap, p->version),
				COND_SWAP32(msg_swap, p->topology),
				COND_SWAP32(msg_swap, p->last_activity_sec), COND_SWAP32(msg_swap, p->last_activity_usec));
		}
		break;
	case TNWG_DSTATTYPE_RM_OTHERGW_NBR:
		{
			tnwg_rm_stat_grp_msg_t * msg;
			tnwg_rm_stat_grp_othergw_neighbor_t *p;
			char * d_or_g;
			uint64_t id;
			uint32_t cost;

			msg = (tnwg_rm_stat_grp_msg_t *) aligned_msg_buffer;

			p = &msg->msgtype.neighbor;
			id = COND_SWAP64(msg_swap, p->node_id);
			cost = COND_SWAP32(msg_swap, p->ingress_cost);
			d_or_g = (COND_SWAP32(msg_swap,p->domain_or_gateway) == TNWG_DSTAT_Domain_Type) ? "Domain" : "Gateway";
			if( verbose_flag == 1){
				printf("\n------------Adjacent %s ID: %"PRIu64"  (Cost: %d) (Version: %d)------------\n%s Sent\n", d_or_g, id, cost, msg_version, time_buff_sent);
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"\"TNWG_DSTATTYPE_RM_OTHERGW_NBR\":{"
				"\"domain_or_gateway\":\"%s\",\"node_id\":\"%"PRIu64"\",\"ingress_cost\":%d,\"version\":%d,\"time_buff_sent\":\"%s\"}"
				,d_or_g, id, cost, msg_version, time_buff_sent);
		}
		break;	
	case TNWG_DSTATTYPE_PORTSTAT:
		{
			tnwg_dstat_portalstats_msg_t *msg_p;
			uint32_t portal_type;
			char *temp_char_buff2 = malloc(ArrayLength);
			int max_str_sz;
			int j;

			msg_p = (tnwg_dstat_portalstats_msg_t *) aligned_msg_buffer;
			portal_type = COND_SWAP32(msg_swap, msg_p->rechdr.portal_type);
			
			if( verbose_flag == 1){
				printf("\n============Portal Statistics for Portal %s(Version: %d)============\n%s Sent\n", msg_p->rechdr.portal_name, msg_version, time_buff_sent);
			}
			lbm_snprintf(TNWG_JSON_buffer, ArrayLength,"\"TNWG_DSTATTYPE_PORTSTAT\":{\"portal_name\":\"%s\",\"version\":%d,\"time_buff_sent\":\"%s\",",msg_p->rechdr.portal_name, msg_version, time_buff_sent);
			j = strlen(TNWG_JSON_buffer);
			max_str_sz = ArrayLength - j - 10;
			if(max_str_sz <= 0 || j <= 0) {
				printf("\n WARNING! TNWG_DSTAT_Portal_Type_Peer: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
				max_str_sz = 0;
			}

			if(portal_type == TNWG_DSTAT_Portal_Type_Peer){
				if( verbose_flag == 1){
					printf("..............................Portal Entry Stats for Type = Peer..............................\n");
					printf("                                                          ingress_cost: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.ingress_cost));
					printf("                                                 local_interest_topics: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.local_interest_topics));
					printf("                                          local_interest_pcre_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.local_interest_pcre_patterns));
					printf("                                         local_interest_regex_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.local_interest_regex_patterns));
					printf("                                                remote_interest_topics: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.remote_interest_topics));
					printf("                                         remote_interest_pcre_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.remote_interest_pcre_patterns));
					printf("                                        remote_interest_regex_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.remote_interest_regex_patterns));
					printf("                                                       proxy_receivers: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.proxy_receivers));
					printf("                                                       receiver_topics: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.receiver_topics));
					printf("                                                receiver_pcre_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.receiver_pcre_patterns));
					printf("                                               receiver_regex_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.receiver_regex_patterns));
					printf("                                                         proxy_sources: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.peer.proxy_sources));
				}

                                lbm_snprintf(temp_char_buff2, max_str_sz,"\"TNWG_DSTAT_Portal_Type_Peer\":{"
						"\"ingress_cost\":\"%"PRIu64"\","
                                                "\"local_interest_topics\":\"%"PRIu64"\","
                                                "\"local_interest_pcre_patterns\":\"%"PRIu64"\","
                                                "\"local_interest_regex_patterns\":\"%"PRIu64"\","
                                                "\"remote_interest_topics\":\"%"PRIu64"\","
                                                "\"remote_interest_pcre_patterns\":\"%"PRIu64"\","
                                                "\"remote_interest_regex_patterns\":\"%"PRIu64"\","
                                                "\"proxy_receivers\":\"%"PRIu64"\","
                                                "\"receiver_topics\":\"%"PRIu64"\","
                                                "\"receiver_pcre_patterns\":\"%"PRIu64"\","
                                                "\"receiver_regex_patterns\":\"%"PRIu64"\","
                                                "\"proxy_sources\":\"%"PRIu64"\"},",

						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.ingress_cost),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.local_interest_topics),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.local_interest_pcre_patterns),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.local_interest_regex_patterns),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.remote_interest_topics),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.remote_interest_pcre_patterns),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.remote_interest_regex_patterns),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.proxy_receivers),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.receiver_topics),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.receiver_pcre_patterns),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.receiver_regex_patterns),
						COND_SWAP64(msg_swap,msg_p->record.ptype.peer.proxy_sources));
				strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);
				j = strlen(TNWG_JSON_buffer);
				max_str_sz = ArrayLength - j;
				if(max_str_sz <= 0 || j <= 0) {
					printf("\n WARNING! TNWG_DSTAT_Portal_Type_Peer: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
					max_str_sz = 0;
				}
								
				{
					tnwg_dstat_peer_receive_stats_t * p;
					p = &msg_p->record.ptype.peer.receive_stats;
					if( verbose_flag == 1){
						printf("..............................Receive Statistics for Type = Peer..............................\n");
						printf("                                                 Data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_msgs_rcvd));
						printf("                                                    Data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_msg_bytes_rcvd));
						printf("                      Transport topic fragment data messages received  : %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msgs_rcvd));
						printf("                  Transport topic fragment data messages received bytes: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msg_bytes_rcvd));
						printf("    Transport topic fragment data messages received with unknown source: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msgs_rcvd_unknown_source));
						printf("       Transport topic fragment data bytes received with unknown source: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msg_bytes_rcvd_unknown_source));
						printf("                Transport topic request fragment data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msgs_rcvd));
						printf("                   Transport topic request fragment data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msg_bytes_rcvd));
						printf("   Transport topic req frag. data messages received with unknown source: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msgs_rcvd_unknown_source));
						printf("      Transport topic req frag. data bytes received with unknown source: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msg_bytes_rcvd_unknown_source));
						printf("                              Transport topic control messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_control_msgs_rcvd));
						printf("                                 Transport topic control bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_control_msg_bytes_rcvd));
						printf("          Transport topic control messages received with unknown source: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_control_msgs_rcvd_unknown_source));
						printf("             Transport topic control bytes received with unknown source: %"PRIu64"\n", COND_SWAP64(msg_swap,p->transport_topic_control_msg_bytes_rcvd_unknown_source));
						printf("                        Immediate topic fragment data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topic_fragment_data_msgs_rcvd));
						printf("                           Immediate topic fragment data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topic_fragment_data_msg_bytes_rcvd));
						printf("                Immediate topic request fragment data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topic_req_fragment_data_msgs_rcvd));
						printf("                   Immediate topic request fragment data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topic_req_fragment_data_msg_bytes_rcvd));
						printf("                    Immediate topicless fragment data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topicless_fragment_data_msgs_rcvd));
						printf("                       Immediate topicless fragment data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topicless_fragment_data_msg_bytes_rcvd));
						printf("            Immediate topicless request fragment data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topicless_req_fragment_data_msgs_rcvd));
						printf("               Immediate topicless request fragment data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->immediate_topicless_req_fragment_data_msg_bytes_rcvd));
						printf("                                         Unicast data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd));
						printf("                                            Unicast data bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd));
						printf("          Unicast data messages received with no forwarding information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd_no_fwd));
						printf("     Unicast data message bytes received with no forwarding information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd_no_fwd));
						printf("     Unicast data messages received with unknown forwarding information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd_unknown_fwd));
						printf("Unicast data message bytes received with unknown forwarding information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd_unknown_fwd));
						printf("        Unicast data messages/bytes received with no stream information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd_no_stream));
						printf("        Unicast data messages/bytes received with no stream information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd_no_stream));
						printf("                                 Unicast data messages dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msgs_dropped_no_route));
						printf("                            Unicast data message bytes dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_dropped_no_route));
						printf("                                        Control messages/bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->cntl_msgs_rcvd));
						printf("                                        Control messages/bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->cntl_msg_bytes_rcvd));
						printf("                                Unicast control messages/bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msgs_rcvd));
						printf("                                Unicast control messages/bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_rcvd));
						printf("                       Unicast Control retransmission requests received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_rxreq_msgs_rcvd));
						printf("                          Unicast Control retransmission bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_rxreq_msg_bytes_rcvd));
						printf("                        Unicast control messages received but unhandled: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msgs_rcvd_unhandled));
						printf("                   Unicast control message bytes received but unhandled: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_rcvd_unhandled));
						printf("           Unicast control messages received with no stream information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msgs_rcvd_no_stream));
						printf("      Unicast control message bytes received with no stream information: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_rcvd_no_stream));
						printf("                              Unicast control messages dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msgs_dropped_no_route));
						printf("                         Unicast control message bytes dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_dropped_no_route));
						printf("                                      Gateway control messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msgs_rcvd));
						printf("                                 Gateway control message bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_rcvd));
					}

					lbm_snprintf(temp_char_buff2, ArrayLength,"\"Peer_Receive_Stats\":{"
						"\"data_msgs_rcvd\":\"%"PRIu64"\","
						"\"data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_fragment_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_fragment_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_fragment_data_msgs_rcvd_unknown_source\":\"%"PRIu64"\","
						"\"transport_topic_fragment_data_msg_bytes_rcvd_unknown_source\":\"%"PRIu64"\","
						"\"transport_topic_req_fragment_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_req_fragment_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_req_fragment_data_msgs_rcvd_unknown_source\":\"%"PRIu64"\","
						"\"transport_topic_req_fragment_data_msg_bytes_rcvd_unknown_source\":\"%"PRIu64"\","
						"\"transport_topic_control_msgs_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_control_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"transport_topic_control_msgs_rcvd_unknown_source\":\"%"PRIu64"\","
						"\"transport_topic_control_msg_bytes_rcvd_unknown_source\":\"%"PRIu64"\","
						"\"immediate_topic_fragment_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"immediate_topic_fragment_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"immediate_topic_req_fragment_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"immediate_topic_req_fragment_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"immediate_topicless_fragment_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"immediate_topicless_fragment_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"immediate_topicless_req_fragment_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"immediate_topicless_req_fragment_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"unicast_data_msgs_rcvd\":\"%"PRIu64"\","
						"\"unicast_data_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"unicast_data_msgs_rcvd_no_fwd\":\"%"PRIu64"\","
						"\"unicast_data_msg_bytes_rcvd_no_fwd\":\"%"PRIu64"\","
						"\"unicast_data_msgs_rcvd_unknown_fwd\":\"%"PRIu64"\","
						"\"unicast_data_msg_bytes_rcvd_unknown_fwd\":\"%"PRIu64"\","
						"\"unicast_data_msgs_rcvd_no_stream\":\"%"PRIu64"\","
						"\"unicast_data_msg_bytes_rcvd_no_stream\":\"%"PRIu64"\","
						"\"unicast_data_msgs_dropped_no_route\":\"%"PRIu64"\","
						"\"unicast_data_msg_bytes_dropped_no_route\":\"%"PRIu64"\","
						"\"cntl_msgs_rcvd\":\"%"PRIu64"\","
						"\"cntl_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"unicast_cntl_msgs_rcvd\":\"%"PRIu64"\","
						"\"unicast_cntl_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"unicast_cntl_rxreq_msgs_rcvd\":\"%"PRIu64"\","
						"\"unicast_cntl_rxreq_msg_bytes_rcvd\":\"%"PRIu64"\","
						"\"unicast_cntl_msgs_rcvd_unhandled\":\"%"PRIu64"\","
						"\"unicast_cntl_msg_bytes_rcvd_unhandled\":\"%"PRIu64"\","
						"\"unicast_cntl_msgs_rcvd_no_stream\":\"%"PRIu64"\","
						"\"unicast_cntl_msg_bytes_rcvd_no_stream\":\"%"PRIu64"\","
						"\"unicast_cntl_msgs_dropped_no_route\":\"%"PRIu64"\","
						"\"unicast_cntl_msg_bytes_dropped_no_route\":\"%"PRIu64"\","
						"\"gateway_cntl_msgs_rcvd\":\"%"PRIu64"\","
						"\"gateway_cntl_msg_bytes_rcvd\":\"%"PRIu64"\"}",

						COND_SWAP64(msg_swap,p->data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msgs_rcvd_unknown_source),
						COND_SWAP64(msg_swap,p->transport_topic_fragment_data_msg_bytes_rcvd_unknown_source),
						COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msgs_rcvd_unknown_source),
						COND_SWAP64(msg_swap,p->transport_topic_req_fragment_data_msg_bytes_rcvd_unknown_source),
						COND_SWAP64(msg_swap,p->transport_topic_control_msgs_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_control_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->transport_topic_control_msgs_rcvd_unknown_source),
						COND_SWAP64(msg_swap,p->transport_topic_control_msg_bytes_rcvd_unknown_source),
						COND_SWAP64(msg_swap,p->immediate_topic_fragment_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topic_fragment_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topic_req_fragment_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topic_req_fragment_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topicless_fragment_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topicless_fragment_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topicless_req_fragment_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->immediate_topicless_req_fragment_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd),
						COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd_no_fwd),
						COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd_no_fwd),
						COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd_unknown_fwd),
						COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd_unknown_fwd),
						COND_SWAP64(msg_swap,p->unicast_data_msgs_rcvd_no_stream),
						COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_rcvd_no_stream),
						COND_SWAP64(msg_swap,p->unicast_data_msgs_dropped_no_route),
						COND_SWAP64(msg_swap,p->unicast_data_msg_bytes_dropped_no_route),
						COND_SWAP64(msg_swap,p->cntl_msgs_rcvd),
						COND_SWAP64(msg_swap,p->cntl_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->unicast_cntl_msgs_rcvd),
						COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->unicast_cntl_rxreq_msgs_rcvd),
						COND_SWAP64(msg_swap,p->unicast_cntl_rxreq_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,p->unicast_cntl_msgs_rcvd_unhandled),
						COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_rcvd_unhandled),
						COND_SWAP64(msg_swap,p->unicast_cntl_msgs_rcvd_no_stream),
						COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_rcvd_no_stream),
						COND_SWAP64(msg_swap,p->unicast_cntl_msgs_dropped_no_route),
						COND_SWAP64(msg_swap,p->unicast_cntl_msg_bytes_dropped_no_route),
						COND_SWAP64(msg_swap,p->gateway_cntl_msgs_rcvd),
						COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_rcvd));
					strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);

					j = strlen(TNWG_JSON_buffer);
					max_str_sz = ArrayLength - j;
					if(max_str_sz <= 0 || j <= 0) {
						printf("\n WARNING! TNWG_DSTAT_Portal_Type_Peer: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
						max_str_sz = 0;
					}

				}
				{
					tnwg_dstat_peer_send_stats_t * p;
					uint64_t rtt_samples;
					time_t last_ka_time;
					char timestring[64];
					size_t sz;

					p = &msg_p->record.ptype.peer.send_stats;
					if( verbose_flag == 1){
						printf("..............................Send Statistics for Type = Peer.................................\n");
						printf("                                  Data fragments forwarded to this portal: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragments_forwarded));
						printf("                             Data fragment bytes forwarded to this portal: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragment_bytes_forwarded));
						printf("                                                      Data fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragments_sent));
						printf("                                                 Data fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragment_bytes_sent));
						printf("                                         Duplicate data fragments dropped: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragments_dropped_dup));
						printf("                                    Duplicate data fragment bytes dropped: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_dup));
						printf("                                Data fragments dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragments_dropped_would_block));
						printf("                           Data fragment bytes dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_would_block));
						printf("               Data fragments dropped due to portal not being operational: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragments_dropped_not_operational));
						printf("          Data fragment bytes dropped due to portal not being operational: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_not_operational));
						printf("                           Data fragments dropped due to queueing failure: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragments_dropped_queue_failure));
						printf("                      Data fragment bytes dropped due to queueing failure: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_queue_failure));
						printf("                                Unicast messages forwarded to this portal: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msgs_forwarded));
						printf("                           Unicast message bytes forwarded to this portal: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msg_bytes_forwarded));
						printf("                                                    Unicast messages sent: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msgs_sent));
						printf("                                               Unicast message bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msg_bytes_sent));
						printf("                              Unicast messages dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msgs_dropped_would_block));
						printf("                         Unicast message bytes dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msg_bytes_dropped_would_block));
						printf("             Unicast messages dropped due to portal not being operational: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msgs_dropped_not_operational));
						printf("        Unicast message bytes dropped due to portal not being operational: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msg_bytes_dropped_not_operational));
						printf("                         Unicast messages dropped due to queueing failure: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msgs_dropped_queue_failure));
						printf("                    Unicast message bytes dropped due to queueing failure: %"PRIu64"\n", COND_SWAP64(msg_swap,p->unicast_msg_bytes_dropped_queue_failure));
						printf("                                                 Gateway control messages: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msgs));
						printf("                                            Gateway control message bytes: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes));
						printf("                                            Gateway control messages sent: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msgs_sent));
						printf("                                       Gateway control message bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_sent));
						printf("                      Gateway control messages dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msgs_dropped_would_block));
						printf("                 Gateway control message bytes dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_dropped_would_block));
						printf("     Gateway control messages dropped due to portal not being operational: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msgs_dropped_not_operational));
						printf("Gateway control message bytes dropped due to portal not being operational: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_dropped_not_operational));
						printf("                 Gateway control messages dropped due to queueing failure: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msgs_dropped_queue_failure));
						printf("            Gateway control message bytes dropped due to queueing failure: %"PRIu64"\n", COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_dropped_queue_failure));
						printf("                                                Number of message batches: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batches));
						printf("                                     Minimum number of messages per batch: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batch_msgs_min));
						printf("                                        Mean number of messages per batch: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batch_msgs_mean)); 
						printf("                                     Maximum number of messages per batch: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batch_msgs_max));
						printf("                                        Minimum number of bytes per batch: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batch_bytes_min));
						printf("                                           Mean number of bytes per batch: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batch_bytes_mean));
						printf("                                        Maximum number of bytes per batch: %"PRIu64"\n", COND_SWAP64(msg_swap,p->batch_bytes_max));
						printf("                                   Current data bytes enqueued internally: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_bytes_enqueued));
						printf("                                   Maximum data bytes enqueued internally: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_bytes_enqueued_max));
						printf("                          Configured maximum data bytes allowed in queued: %"PRIu64"\n", COND_SWAP64(msg_swap,p->data_bytes_enqueued_limit));
						printf("                                                        Total RTT samples: %"PRIu64"\n", COND_SWAP64(msg_swap,p->rtt_samples));

					}

                                        lbm_snprintf(temp_char_buff2, max_str_sz,",\"Peer_Send_Stats\":{"
                                                "\"data_fragments_forwarded\":\"%"PRIu64"\","
                                                "\"data_fragment_bytes_forwarded\":\"%"PRIu64"\","
                                                "\"data_fragments_sent\":\"%"PRIu64"\","
                                                "\"data_fragment_bytes_sent\":\"%"PRIu64"\","
                                                "\"data_fragments_dropped_dup\":\"%"PRIu64"\","
                                                "\"data_fragment_bytes_dropped_dup\":\"%"PRIu64"\","
                                                "\"data_fragments_dropped_would_block\":\"%"PRIu64"\","
                                                "\"data_fragment_bytes_dropped_would_block\":\"%"PRIu64"\","
                                                "\"data_fragments_dropped_not_operational\":\"%"PRIu64"\","
                                                "\"data_fragment_bytes_dropped_not_operational\":\"%"PRIu64"\","
                                                "\"data_fragments_dropped_queue_failure\":\"%"PRIu64"\","
                                                "\"data_fragment_bytes_dropped_queue_failure\":\"%"PRIu64"\","
                                                "\"unicast_msgs_forwarded\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_forwarded\":\"%"PRIu64"\","
                                                "\"unicast_msgs_sent\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_sent\":\"%"PRIu64"\","
                                                "\"unicast_msgs_dropped_would_block\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_dropped_would_block\":\"%"PRIu64"\","
                                                "\"unicast_msgs_dropped_not_operational\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_dropped_not_operational\":\"%"PRIu64"\","
                                                "\"unicast_msgs_dropped_queue_failure\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_dropped_queue_failure\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msgs\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msg_bytes\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msgs_sent\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msg_bytes_sent\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msgs_dropped_would_block\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msg_bytes_dropped_would_block\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msgs_dropped_not_operational\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msg_bytes_dropped_not_operational\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msgs_dropped_queue_failure\":\"%"PRIu64"\","
                                                "\"gateway_cntl_msg_bytes_dropped_queue_failure\":\"%"PRIu64"\","
                                                "\"batches\":\"%"PRIu64"\","
                                                "\"batch_msgs_min\":\"%"PRIu64"\","
                                                "\"batch_msgs_mean\":\"%"PRIu64"\","
                                                "\"batch_msgs_max\":\"%"PRIu64"\","
                                                "\"batch_bytes_min\":\"%"PRIu64"\","
                                                "\"batch_bytes_mean\":\"%"PRIu64"\","
                                                "\"batch_bytes_max\":\"%"PRIu64"\","
                                                "\"data_bytes_enqueued\":\"%"PRIu64"\","
                                                "\"data_bytes_enqueued_max\":\"%"PRIu64"\","
                                                "\"data_bytes_enqueued_limit\":\"%"PRIu64"\","
                                                "\"rtt_samples\":\"%"PRIu64"\"",

						COND_SWAP64(msg_swap,p->data_fragments_forwarded),
						COND_SWAP64(msg_swap,p->data_fragment_bytes_forwarded),
						COND_SWAP64(msg_swap,p->data_fragments_sent),
						COND_SWAP64(msg_swap,p->data_fragment_bytes_sent),
						COND_SWAP64(msg_swap,p->data_fragments_dropped_dup),
						COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_dup),
						COND_SWAP64(msg_swap,p->data_fragments_dropped_would_block),
						COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_would_block),
						COND_SWAP64(msg_swap,p->data_fragments_dropped_not_operational),
						COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_not_operational),
						COND_SWAP64(msg_swap,p->data_fragments_dropped_queue_failure),
						COND_SWAP64(msg_swap,p->data_fragment_bytes_dropped_queue_failure),
						COND_SWAP64(msg_swap,p->unicast_msgs_forwarded),
						COND_SWAP64(msg_swap,p->unicast_msg_bytes_forwarded),
						COND_SWAP64(msg_swap,p->unicast_msgs_sent),
						COND_SWAP64(msg_swap,p->unicast_msg_bytes_sent),
						COND_SWAP64(msg_swap,p->unicast_msgs_dropped_would_block),
						COND_SWAP64(msg_swap,p->unicast_msg_bytes_dropped_would_block),
						COND_SWAP64(msg_swap,p->unicast_msgs_dropped_not_operational),
						COND_SWAP64(msg_swap,p->unicast_msg_bytes_dropped_not_operational),
						COND_SWAP64(msg_swap,p->unicast_msgs_dropped_queue_failure),
						COND_SWAP64(msg_swap,p->unicast_msg_bytes_dropped_queue_failure),
						COND_SWAP64(msg_swap,p->gateway_cntl_msgs),
						COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes),
						COND_SWAP64(msg_swap,p->gateway_cntl_msgs_sent),
						COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_sent),
						COND_SWAP64(msg_swap,p->gateway_cntl_msgs_dropped_would_block),
						COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_dropped_would_block),
						COND_SWAP64(msg_swap,p->gateway_cntl_msgs_dropped_not_operational),
						COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_dropped_not_operational),
						COND_SWAP64(msg_swap,p->gateway_cntl_msgs_dropped_queue_failure),
						COND_SWAP64(msg_swap,p->gateway_cntl_msg_bytes_dropped_queue_failure),
						COND_SWAP64(msg_swap,p->batches),
						COND_SWAP64(msg_swap,p->batch_msgs_min),
						COND_SWAP64(msg_swap,p->batch_msgs_mean), 
						COND_SWAP64(msg_swap,p->batch_msgs_max),
						COND_SWAP64(msg_swap,p->batch_bytes_min),
						COND_SWAP64(msg_swap,p->batch_bytes_mean),
						COND_SWAP64(msg_swap,p->batch_bytes_max),
						COND_SWAP64(msg_swap,p->data_bytes_enqueued),
						COND_SWAP64(msg_swap,p->data_bytes_enqueued_max),
						COND_SWAP64(msg_swap,p->data_bytes_enqueued_limit),
						COND_SWAP64(msg_swap,p->rtt_samples));
						strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);

						j = strlen(TNWG_JSON_buffer);
						max_str_sz = ArrayLength - j;
						if(max_str_sz <= 0 || j <= 0) {
							printf("\n WARNING! TNWG_DSTAT_Portal_Type_Peer: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
							max_str_sz = 0;
						}
						rtt_samples = COND_SWAP64(msg_swap,p->rtt_samples);
						if(rtt_samples == 0){
							if( verbose_flag == 1){
								printf("                                                 Minimum RTT to companion: N/A\n");
								printf("                                                    Mean RTT to companion: N/A\n");
								printf("                                                 Maximum RTT to companion: N/A\n");
							} 
							lbm_snprintf(temp_char_buff2, max_str_sz,",\"rtt_min\":\"0\",\"rtt_mean\":\"0\",\"rtt_max\":\"0\"");
						} else {
							if( verbose_flag == 1){
								printf("                                   Minimum RTT to companion(microseconds): %"PRId64"\n", COND_SWAP64(msg_swap,p->rtt_min));
								printf("                                      Mean RTT to companion(microseconds): %"PRIu64"\n", COND_SWAP64(msg_swap,p->rtt_mean));
								printf("                                   Maximum RTT to companion(microseconds): %"PRIu64"\n", COND_SWAP64(msg_swap,p->rtt_max));
							} 
							lbm_snprintf(temp_char_buff2, max_str_sz,",\"rtt_min\":\"%"PRId64"\",\"rtt_mean\":\"%"PRId64"\",\"rtt_max\":\"%"PRId64"\"");
						}
						strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);

						j = strlen(TNWG_JSON_buffer);
						max_str_sz = ArrayLength - j;
						if(max_str_sz <= 0 || j <= 0) {
							printf("\n WARNING! TNWG_DSTAT_Portal_Type_Peer: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
							max_str_sz = 0;
						}

						last_ka_time = COND_SWAP64(msg_swap,p->last_ka_time);

						if(last_ka_time == 0){
							if( verbose_flag == 1){
								printf("                                              Last keepalive responded to: None\n");
							}
							lbm_snprintf(temp_char_buff2, max_str_sz,",\"last_ka_time\":\"None\"}}");
						} else {
							struct tm *gm = gmtime(&last_ka_time);
							if(gm){
								sz = (size_t)strftime(timestring, sizeof(timestring), "%Y-%m-%d %H:%M:%S GMT", gm);
								if(sz){
									if( verbose_flag == 1){
										printf("                                              Last keepalive responded to: %s\n", timestring);
									}
									lbm_snprintf(temp_char_buff2, max_str_sz,",\"last_ka_time\":\"%s\"}}");
								}else{
									if( verbose_flag == 1){
										printf("                                              Last keepalive responded to: unknown\n");
									}
									lbm_snprintf(temp_char_buff2, max_str_sz,",\"last_ka_time\":\"unknown\"}}");
								}
							} else {
									if( verbose_flag == 1){
										printf("                                              Last keepalive responded to: unknown\n");
									}
									lbm_snprintf(temp_char_buff2, max_str_sz,",\"last_ka_time\":\"unknown\"}}");
							}								
						}

						strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);
						j = strlen(TNWG_JSON_buffer);
						max_str_sz = ArrayLength - j;
						if(max_str_sz <= 0 || j <= 0) {
							printf("\n WARNING! TNWG_DSTAT_Portal_Type_Peer: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
							max_str_sz = 0;
						}
					}
			} else {	/* Else type = Endpoint */	
				tnwg_dstat_endpoint_receive_stats_t * rec_p;
				tnwg_dstat_endpoint_send_stats_t * sen_p;

				if( verbose_flag == 1){
					printf("...............Portal Entry Stats for Type = EndPoint...............\n");
					printf("                      domain_id: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.domain_id));	
					printf("                   ingress_cost: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.ingress_cost));
					printf("          local_interest_topics: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.local_interest_topics));
					printf("   local_interest_pcre_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.local_interest_pcre_patterns));
					printf("  local_interest_regex_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.local_interest_regex_patterns));
					printf("         remote_interest_topics: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.remote_interest_topics));
					printf("  remote_interest_pcre_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.remote_interest_pcre_patterns));
					printf(" remote_interest_regex_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.remote_interest_regex_patterns));
					printf("                proxy_receivers: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.proxy_receivers));
					printf("                receiver_topics: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.receiver_topics));
					printf("         receiver_pcre_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.receiver_pcre_patterns));
					printf("        receiver_regex_patterns: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.receiver_regex_patterns));
					printf("                  proxy_sources: %"PRIu64"\n", COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.proxy_sources));
					printf("\n...............Receive Statistics for Type = EndPoint...............\n");
				}

                                lbm_snprintf(temp_char_buff2, max_str_sz, "\"Portal_Entry_Stats_Endpoint\":{"
                                        "\"domain_id\":\"%"PRIu64"\","
                                        "\"ingress_cost\":\"%"PRIu64"\","
                                        "\"local_interest_topics\":\"%"PRIu64"\","
                                        "\"local_interest_pcre_patterns\":\"%"PRIu64"\","
                                        "\"local_interest_regex_patterns\":\"%"PRIu64"\","
                                        "\"remote_interest_topics\":\"%"PRIu64"\","
                                        "\"remote_interest_pcre_patterns\":\"%"PRIu64"\","
                                        "\"remote_interest_regex_patterns\":\"%"PRIu64"\","
                                        "\"proxy_receivers\":\"%"PRIu64"\","
                                        "\"receiver_topics\":\"%"PRIu64"\","
                                        "\"receiver_pcre_patterns\":\"%"PRIu64"\","
                                        "\"receiver_regex_patterns\":\"%"PRIu64"\","
                                        "\"proxy_sources\":\"%"PRIu64"\"}",

					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.domain_id),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.ingress_cost),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.local_interest_topics),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.local_interest_pcre_patterns),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.local_interest_regex_patterns),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.remote_interest_topics),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.remote_interest_pcre_patterns),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.remote_interest_regex_patterns),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.proxy_receivers),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.receiver_topics),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.receiver_pcre_patterns),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.receiver_regex_patterns),
					COND_SWAP64(msg_swap,msg_p->record.ptype.endpt.proxy_sources));
				
				strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);

				j = strlen(TNWG_JSON_buffer);
				max_str_sz = ArrayLength - j;
				if(max_str_sz <= 0 || j <= 0) {
					printf("\n WARNING! Portal_Entry_Stats_Endpoint: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
					max_str_sz = 0;
				}
				rec_p = &msg_p->record.ptype.endpt.receive_stats;
				if( verbose_flag == 1){
						printf("                            Transport topic message fragments received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->transport_topic_fragments_rcvd));
						printf("                       Transport topic message fragment bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->transport_topic_fragment_bytes_rcvd));
						printf("                    Transport topic message request fragments received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->transport_topic_req_fragments_rcvd));
						printf("               Transport topic message request fragment bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->transport_topic_req_fragment_bytes_rcvd));
						printf("                              Transport topic control message received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->transport_topic_control_rcvd));
						printf("                        Transport topic control message bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->transport_topic_control_bytes_rcvd));
						printf("                            Immediate topic message fragments received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topic_fragments_rcvd));
						printf("                       Immediate topic message fragment bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topic_fragment_bytes_rcvd));
						printf("                    Immediate topic message request fragments received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topic_req_fragments_rcvd));
						printf("               Immediate topic message request fragment bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topic_req_fragment_bytes_rcvd));
						printf("                        Immediate topicless message fragments received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topicless_fragments_rcvd));
						printf("                   Immediate topicless message fragment bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topicless_fragment_bytes_rcvd));
						printf("                Immediate topicless message request fragments received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topicless_req_fragments_rcvd));
						printf("           Immediate topicless message request fragment bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->immediate_topicless_req_fragment_bytes_rcvd));
						printf("                                        Unicast data messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_rcvd));
						printf("                                   Unicast data message bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_rcvd));
						printf("          Unicast data messages received with no stream identification: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_rcvd_no_stream));
						printf("     Unicast data message bytes received with no stream identification: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_rcvd_no_stream));
						printf("                           Unicast data messages dropped as duplicates: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_dropped_dup));
						printf("                      Unicast data message bytes dropped as duplicates: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_dropped_dup));
						printf("                                Unicast data messages dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_dropped_no_route));
						printf("                           Unicast data message bytes dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_dropped_no_route));
						printf("                                     Unicast control messages received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_rcvd));
						printf("                                Unicast control message bytes received: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_rcvd));
						printf("       Unicast control messages received with no stream identification: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_rcvd_no_stream));
						printf("  Unicast control message bytes received with no stream identification: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_rcvd_no_stream));
						printf("                        Unicast control messages dropped as duplicates: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_dropped_dup));
						printf("                   Unicast control message bytes dropped as duplicates: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_dropped_dup));
						printf("                             Unicast control messages dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_dropped_no_route));
						printf("                        Unicast control message bytes dropped no route: %"PRIu64"\n", COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_dropped_no_route));
				}

                                lbm_snprintf(temp_char_buff2, ArrayLength,"\"Endpoint_Receive_Stats\":{"
                                                "\"transport_topic_fragments_rcvd\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"transport_topic_req_fragments_rcvd\":\"%"PRIu64"\","
                                                "\"transport_topic_req_fragment_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"transport_topic_control_rcvd\":\"%"PRIu64"\","
                                                "\"transport_topic_control_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragments_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragment_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topic_req_fragments_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topic_req_fragment_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragments_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragment_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topicless_req_fragments_rcvd\":\"%"PRIu64"\","
                                                "\"immediate_topicless_req_fragment_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"unicast_data_msgs_rcvd\":\"%"PRIu64"\","
                                                "\"unicast_data_msg_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"unicast_data_msgs_rcvd_no_stream\":\"%"PRIu64"\","
                                                "\"unicast_data_msg_bytes_rcvd_no_stream\":\"%"PRIu64"\","
                                                "\"unicast_data_msgs_dropped_dup\":\"%"PRIu64"\","
                                                "\"unicast_data_msg_bytes_dropped_dup\":\"%"PRIu64"\","
                                                "\"unicast_data_msgs_dropped_no_route\":\"%"PRIu64"\","
                                                "\"unicast_data_msg_bytes_dropped_no_route\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msgs_rcvd\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msg_bytes_rcvd\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msgs_rcvd_no_stream\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msg_bytes_rcvd_no_stream\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msgs_dropped_dup\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msg_bytes_dropped_dup\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msgs_dropped_no_route\":\"%"PRIu64"\","
                                                "\"unicast_cntl_msg_bytes_dropped_no_route\":\"%"PRIu64"\"}",

						COND_SWAP64(msg_swap,rec_p->transport_topic_fragments_rcvd),
						COND_SWAP64(msg_swap,rec_p->transport_topic_fragment_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->transport_topic_req_fragments_rcvd),
						COND_SWAP64(msg_swap,rec_p->transport_topic_req_fragment_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->transport_topic_control_rcvd),
						COND_SWAP64(msg_swap,rec_p->transport_topic_control_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topic_fragments_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topic_fragment_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topic_req_fragments_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topic_req_fragment_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topicless_fragments_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topicless_fragment_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topicless_req_fragments_rcvd),
						COND_SWAP64(msg_swap,rec_p->immediate_topicless_req_fragment_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_rcvd),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_rcvd_no_stream),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_rcvd_no_stream),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_dropped_dup),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_dropped_dup),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msgs_dropped_no_route),
						COND_SWAP64(msg_swap,rec_p->unicast_data_msg_bytes_dropped_no_route),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_rcvd),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_rcvd),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_rcvd_no_stream),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_rcvd_no_stream),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_dropped_dup),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_dropped_dup),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msgs_dropped_no_route),
						COND_SWAP64(msg_swap,rec_p->unicast_cntl_msg_bytes_dropped_no_route));
				strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);
				j = strlen(TNWG_JSON_buffer);
				max_str_sz = ArrayLength - j;
				if(max_str_sz <= 0 || j <= 0) {
					printf("\n WARNING! Portal_Entry_Stats_Endpoint: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
					max_str_sz = 0;
				}
				sen_p = &msg_p->record.ptype.endpt.send_stats;
				if( verbose_flag == 1){
						printf("\n...............Send Statistics for Type = EndPoint..................\n");
						printf("                       Transport topic fragments forwarded to this portal: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_forwarded));
						printf("                  Transport topic fragment bytes forwarded to this portal: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_forwarded));
						printf("                                           Transport topic fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_sent));
						printf("                                      Transport topic fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_sent));
						printf("                                   Transport topic request fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_req_fragments_sent));
						printf("                              Transport topic request fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_req_fragment_bytes_sent));
						printf("                              Duplicate transport topic fragments dropped: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_dup));
						printf("                         Duplicate transport topic fragment bytes dropped: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_dup));
						printf("                     Transport topic fragments dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_would_block));
						printf("                Transport topic fragment bytes dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_would_block));
						printf("                           Transport topic fragments dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_error));
						printf("                      Transport topic fragment bytes dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_error));
						printf("             Transport topic fragments dropped due to fragment size error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_size_error));
						printf("              Transport topic fragment dropped due to fragment size error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_size_error));
						printf("                                      Immediate topic fragments forwarded: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_forwarded));
						printf("                                 Immediate topic fragment bytes forwarded: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_forwarded));
						printf("                                           Immediate topic fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_sent));
						printf("                                      Immediate topic fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_sent));
						printf("                                   Immediate topic request fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_req_fragments_sent));
						printf("                              Immediate topic request fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_req_fragment_bytes_sent));
						printf("                     Immediate topic fragments dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_dropped_would_block));
						printf("                Immediate topic fragment bytes dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_dropped_would_block));
						printf("                           Immediate topic fragments dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_dropped_error));
						printf("                      Immediate topic fragment bytes dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_dropped_error));
						printf("             Immediate topic fragments dropped due to fragment size error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_dropped_size_error));
						printf("        Immediate topic fragment bytes dropped due to fragment size error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_dropped_size_error));
						printf("                                  Immediate topicless fragments forwarded: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_forwarded));
						printf("                             Immediate topicless fragment bytes forwarded: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_forwarded));
						printf("                                       Immediate topicless fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_sent));
						printf("                                  Immediate topicless fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_sent));
						printf("                               Immediate topicless request fragments sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_req_fragments_sent));
						printf("                          Immediate topicless request fragment bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_req_fragment_bytes_sent));
						printf("                 Immediate topicless fragments dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_dropped_would_block));
						printf("            Immediate topicless fragment bytes dropped due to EWOULDBLOCK: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_dropped_would_block));
						printf("                       Immediate topicless fragments dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_dropped_error));
						printf("                  Immediate topicless fragment bytes dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_dropped_error));
						printf("         Immediate topicless fragments dropped due to fragment size error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_dropped_size_error));
						printf("    Immediate topicless fragment bytes dropped due to fragment size error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_dropped_size_error));
						printf("                                               Unicast messages forwarded: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->unicast_msgs_forwarded));
						printf("                                          Unicast message bytes forwarded: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->unicast_msg_bytes_forwarded));
						printf("                                                    Unicast messages sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->unicast_msgs_sent));
						printf("                                               Unicast message bytes sent: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->unicast_msg_bytes_sent));
						printf("                                    Unicast messages dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->unicast_msgs_dropped_error));
						printf("                               Unicast message bytes dropped due to error: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->unicast_msg_bytes_dropped_error));
						printf("                                   Current data bytes enqueued internally: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->data_bytes_enqueued));
						printf("                                   Maximum data bytes enqueued internally: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->data_bytes_enqueued_max));
						printf("                          Configured maximum data bytes allowed in queued: %"PRIu64"\n", COND_SWAP64(msg_swap,sen_p->data_bytes_enqueued_limit));
				}

                                lbm_snprintf(temp_char_buff2, max_str_sz ,"\"Endpoint_Send_Stats\":{"
                                                "\"transport_topic_fragments_forwarded\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_forwarded\":\"%"PRIu64"\","
                                                "\"transport_topic_fragments_sent\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_sent\":\"%"PRIu64"\","
                                                "\"transport_topic_req_fragments_sent\":\"%"PRIu64"\","
                                                "\"transport_topic_req_fragment_bytes_sent\":\"%"PRIu64"\","
                                                "\"transport_topic_fragments_dropped_dup\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_dropped_dup\":\"%"PRIu64"\","
                                                "\"transport_topic_fragments_dropped_would_block\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_dropped_would_block\":\"%"PRIu64"\","
                                                "\"transport_topic_fragments_dropped_error\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_dropped_error\":\"%"PRIu64"\","
                                                "\"transport_topic_fragments_dropped_size_error\":\"%"PRIu64"\","
                                                "\"transport_topic_fragment_bytes_dropped_size_error\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragments_forwarded\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragment_bytes_forwarded\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragments_sent\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragment_bytes_sent\":\"%"PRIu64"\","
                                                "\"immediate_topic_req_fragments_sent\":\"%"PRIu64"\","
                                                "\"immediate_topic_req_fragment_bytes_sent\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragments_dropped_would_block\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragment_bytes_dropped_would_block\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragments_dropped_error\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragment_bytes_dropped_error\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragments_dropped_size_error\":\"%"PRIu64"\","
                                                "\"immediate_topic_fragment_bytes_dropped_size_error\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragments_forwarded\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragment_bytes_forwarded\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragments_sent\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragment_bytes_sent\":%"PRIu64"\","
                                                "\"immediate_topicless_req_fragments_sent\":\"%"PRIu64"\","
                                                "\"immediate_topicless_req_fragment_bytes_sent\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragments_dropped_would_block\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragment_bytes_dropped_would_block\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragments_dropped_error\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragment_bytes_dropped_error\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragments_dropped_size_error\":\"%"PRIu64"\","
                                                "\"immediate_topicless_fragment_bytes_dropped_size_error\":\"%"PRIu64"\","
                                                "\"unicast_msgs_forwarded\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_forwarded\":\"%"PRIu64"\","
                                                "\"unicast_msgs_sent\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_sent\":\"%"PRIu64"\","
                                                "\"unicast_msgs_dropped_error\":\"%"PRIu64"\","
                                                "\"unicast_msg_bytes_dropped_error\":\"%"PRIu64"\","
                                                "\"data_bytes_enqueued\":\"%"PRIu64"\","
                                                "\"data_bytes_enqueued_max\":\"%"PRIu64"\","
                                                "\"data_bytes_enqueued_limit\":\"%"PRIu64"\"}",


						COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_forwarded),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_forwarded),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_sent),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->transport_topic_req_fragments_sent),
						COND_SWAP64(msg_swap,sen_p->transport_topic_req_fragment_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_dup),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_dup),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_would_block),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_would_block),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_error),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_error),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragments_dropped_size_error),
						COND_SWAP64(msg_swap,sen_p->transport_topic_fragment_bytes_dropped_size_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_forwarded),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_forwarded),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_req_fragments_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_req_fragment_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_dropped_would_block),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_dropped_would_block),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_dropped_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_dropped_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragments_dropped_size_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topic_fragment_bytes_dropped_size_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_forwarded),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_forwarded),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_req_fragments_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_req_fragment_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_dropped_would_block),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_dropped_would_block),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_dropped_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_dropped_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragments_dropped_size_error),
						COND_SWAP64(msg_swap,sen_p->immediate_topicless_fragment_bytes_dropped_size_error),
						COND_SWAP64(msg_swap,sen_p->unicast_msgs_forwarded),
						COND_SWAP64(msg_swap,sen_p->unicast_msg_bytes_forwarded),
						COND_SWAP64(msg_swap,sen_p->unicast_msgs_sent),
						COND_SWAP64(msg_swap,sen_p->unicast_msg_bytes_sent),
						COND_SWAP64(msg_swap,sen_p->unicast_msgs_dropped_error),
						COND_SWAP64(msg_swap,sen_p->unicast_msg_bytes_dropped_error),
						COND_SWAP64(msg_swap,sen_p->data_bytes_enqueued),
						COND_SWAP64(msg_swap,sen_p->data_bytes_enqueued_max),
						COND_SWAP64(msg_swap,sen_p->data_bytes_enqueued_limit));
				j = strlen(TNWG_JSON_buffer);
				max_str_sz = ArrayLength - j;
				if(max_str_sz <= 0 || j <= 0) {
					printf("\n WARNING! Portal_Entry_Stats_Endpoint: Truncated Buffer: Increase JSON_buffer in the onReceive() method of LBMDaemonStatsReceiver" );
					max_str_sz = 0;
				}
				strncat(TNWG_JSON_buffer,temp_char_buff2,max_str_sz);

			} /* End of Else type = Endpoint */
			free(temp_char_buff2);
			break;
		}
	default:
		if( verbose_flag == 1){
			printf("unknown message type 0x%x\n", msg_type);
		}
		unknown_msg = 1;
	}

	if( unknown_msg == 1 ){
		return JNI_FALSE;
	} else {
		return JNI_TRUE;
	}
} /* tnwg_dmon_msg_handler() */


void main() {
}

