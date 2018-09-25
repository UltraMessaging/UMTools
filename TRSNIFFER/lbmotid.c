/*
 * FILE: lbmotid.c
 * Author Ibu Akinyemi
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
 * NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 * PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE
 * LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
 * INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
 * TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF
 * THE LIKELIHOOD OF SUCH DAMAGES.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "lbmrpckt.h"


/* The version will be put in the same spot for _all_ types. */
#define LBM_OTID_COMMON_VERSION_OFFSET 31
#define LBM_OTID_VERSION_1 1

#define LBM_OTID_TYPE_OFFSET 0

#define LBM_OTID_TCP_TYPE_OFFSET 0
#define LBM_OTID_TCP_SRC_IP_OFFSET 1
#define LBM_OTID_TCP_SRC_PORT_OFFSET 5
#define LBM_OTID_TCP_TIDX_OFFSET 7
#define LBM_OTID_TCP_CTX_ID_OFFSET 11
#define LBM_OTID_TCP_XPORT_IDX_OFFSET 15
#define LBM_OTID_TCP_SESSION_ID_OFFSET 19

#define LBM_OTID_LBTRM_TYPE_OFFSET 0
#define LBM_OTID_LBTRM_SRC_IP_OFFSET 1
#define LBM_OTID_LBTRM_SRC_PORT_OFFSET 5
#define LBM_OTID_LBTRM_MC_GRP_OFFSET 7
#define LBM_OTID_LBTRM_DEST_PORT_OFFSET 11
#define LBM_OTID_LBTRM_SESSID_OFFSET 13
#define LBM_OTID_LBTRM_TIDX_OFFSET 17
#define LBM_OTID_LBTRM_CTX_ID_OFFSET 21
#define LBM_OTID_LBTRM_XPORT_IDX_OFFSET 25

#define LBM_OTID_LBTRU_TYPE_OFFSET 0
#define LBM_OTID_LBTRU_SRC_IP_OFFSET 1
#define LBM_OTID_LBTRU_SRC_PORT_OFFSET 5
#define LBM_OTID_LBTRU_SESSID_OFFSET 7
#define LBM_OTID_LBTRU_TIDX_OFFSET 11
#define LBM_OTID_LBTRU_CTX_ID_OFFSET 15
#define LBM_OTID_LBTRU_XPORT_IDX_OFFSET 19

#define LBM_OTID_LBTIPC_TYPE_OFFSET 0
#define LBM_OTID_LBTIPC_HOST_ID_OFFSET 1
#define LBM_OTID_LBTIPC_XPORT_ID_OFFSET 5
#define LBM_OTID_LBTIPC_SESSID_OFFSET 7
#define LBM_OTID_LBTIPC_TIDX_OFFSET 11
#define LBM_OTID_LBTIPC_CTX_ID_OFFSET 15
#define LBM_OTID_LBTIPC_XPORT_IDX_OFFSET 19

#define LBM_OTID_LBTSMX_TYPE_OFFSET 0
#define LBM_OTID_LBTSMX_HOST_ID_OFFSET 1
#define LBM_OTID_LBTSMX_XPORT_ID_OFFSET 5
#define LBM_OTID_LBTSMX_SESSID_OFFSET 7
#define LBM_OTID_LBTSMX_TIDX_OFFSET 11
#define LBM_OTID_LBTSMX_CTX_ID_OFFSET 15
#define LBM_OTID_LBTSMX_XPORT_IDX_OFFSET 19

#define LBM_OTID_LBTRDMA_TYPE_OFFSET 0
#define LBM_OTID_LBTRDMA_SRC_IP_OFFSET 1
#define LBM_OTID_LBTRDMA_SRC_PORT_OFFSET 5
#define LBM_OTID_LBTRDMA_SESSID_OFFSET 7
#define LBM_OTID_LBTRDMA_TIDX_OFFSET 11
#define LBM_OTID_LBTRDMA_CTX_ID_OFFSET 15
#define LBM_OTID_LBTRDMA_XPORT_IDX_OFFSET 19

static int lbm_otid_tcp_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info);
static int lbm_otid_lbtrm_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info);
static int lbm_otid_lbtru_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info);
static int lbm_otid_lbtipc_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info);
static int lbm_otid_lbtrdma_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info);


int lbm_otid_tcp_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info)
{
	uint32_t tmp_fld32;
	uint16_t tmp_fld16;

	info->type = LBM_TRANSPORT_TCP;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_TCP_SRC_IP_OFFSET]), 4);
	info->src_ip = tmp_fld32;
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_TCP_SRC_PORT_OFFSET]), 2);
	info->src_port = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_TCP_TIDX_OFFSET]), 4);
	info->topic_idx = tmp_fld32;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_TCP_SESSION_ID_OFFSET]), 4);
	info->session_id = tmp_fld32;
	if (otid[LBM_OTID_COMMON_VERSION_OFFSET] < LBM_OTID_VERSION_1) {
		info->transport_idx = 0; /* OTID does not contain the transport index */
	}
	else {
		memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_TCP_XPORT_IDX_OFFSET]), 4);
		info->transport_idx = tmp_fld32;
	}
	return 0;
}


int lbm_otid_lbtrm_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info)
{
	uint32_t tmp_fld32;
	uint16_t tmp_fld16;

	info->type = LBM_TRANSPORT_LBTRM;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRM_SRC_IP_OFFSET]), 4);
	info->src_ip = tmp_fld32;
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_LBTRM_SRC_PORT_OFFSET]), 2);
	info->src_port = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRM_MC_GRP_OFFSET]), 4);
	info->mc_group = tmp_fld32;
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_LBTRM_DEST_PORT_OFFSET]), 2);
	info->dest_port = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRM_SESSID_OFFSET]), 4);
	info->session_id = tmp_fld32;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRM_TIDX_OFFSET]), 4);
	info->topic_idx = tmp_fld32;
	if (otid[LBM_OTID_COMMON_VERSION_OFFSET] < LBM_OTID_VERSION_1) {
		info->transport_idx = 0; /* OTID does not contain the transport index */
	}
	else {
		memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRM_XPORT_IDX_OFFSET]), 4);
		info->transport_idx = ntohl(tmp_fld32);
	}
	return 0;
}


int lbm_otid_lbtru_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info)
{
	uint32_t tmp_fld32;
	uint16_t tmp_fld16;

	info->type = LBM_TRANSPORT_LBTRU;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRU_SRC_IP_OFFSET]), 4);
	info->src_ip = tmp_fld32;
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_LBTRU_SRC_PORT_OFFSET]), 2);
	info->src_port = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRU_SESSID_OFFSET]), 4);
	info->session_id = tmp_fld32;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRU_TIDX_OFFSET]), 4);
	info->topic_idx = tmp_fld32;
	if (otid[LBM_OTID_COMMON_VERSION_OFFSET] < LBM_OTID_VERSION_1) {
		info->transport_idx = 0; /* OTID does not contain the transport index */
	}
	else {
		memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRU_XPORT_IDX_OFFSET]), 4);
		info->transport_idx = ntohl(tmp_fld32);
	}
	return 0;
}



int lbm_otid_lbtipc_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info)
{
	uint32_t tmp_fld32;
	uint16_t tmp_fld16;

	info->type = LBM_TRANSPORT_LBTIPC;
	/* Ignore the host ID. */
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_LBTIPC_XPORT_ID_OFFSET]), 2);
	info->transport_id = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTIPC_SESSID_OFFSET]), 4);
	info->session_id = tmp_fld32;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTIPC_TIDX_OFFSET]), 4);
	info->topic_idx = tmp_fld32;
	if (otid[LBM_OTID_COMMON_VERSION_OFFSET] < LBM_OTID_VERSION_1) {
		info->transport_idx = 0; /* OTID does not contain the transport index */
	}
	else {
		memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTIPC_XPORT_IDX_OFFSET]), 4);
		info->transport_idx = tmp_fld32;
	}
	return 0;
}

int lbm_otid_lbtsmx_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info)
{
	uint32_t tmp_fld32;
	uint16_t tmp_fld16;

	info->type = LBM_TRANSPORT_LBTSMX;
	/* Ignore the host ID. */
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_LBTSMX_XPORT_ID_OFFSET]), 2);
	info->transport_id = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTSMX_SESSID_OFFSET]), 4);
	info->session_id = tmp_fld32;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTSMX_TIDX_OFFSET]), 4);
	info->topic_idx = tmp_fld32;
	if (otid[LBM_OTID_COMMON_VERSION_OFFSET] < LBM_OTID_VERSION_1) {
		info->transport_idx = 0; /* OTID does not contain the transport index */
	}
	else {
		memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTSMX_XPORT_IDX_OFFSET]), 4);
		info->transport_idx = tmp_fld32;
	}
	return 0;
}


int lbm_otid_lbtrdma_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info)
{
	uint32_t tmp_fld32;
	uint16_t tmp_fld16;

	info->type = LBM_TRANSPORT_LBTRDMA;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRDMA_SRC_IP_OFFSET]), 4);
	info->src_ip = tmp_fld32;
	memcpy((void *) &tmp_fld16, &(otid[LBM_OTID_LBTRDMA_SRC_PORT_OFFSET]), 2);
	info->src_port = tmp_fld16;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRDMA_SESSID_OFFSET]), 4);
	info->session_id = tmp_fld32;
	memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRDMA_TIDX_OFFSET]), 4);
	info->topic_idx = tmp_fld32;
	if (otid[LBM_OTID_COMMON_VERSION_OFFSET] < LBM_OTID_VERSION_1) {
		info->transport_idx = 0; /* OTID does not contain the transport index */
	}
	else {
		memcpy((void *) &tmp_fld32, &(otid[LBM_OTID_LBTRDMA_XPORT_IDX_OFFSET]), 4);
		info->transport_idx = tmp_fld32;
	}
	return 0;
}

int lbm_otid_to_transport_source(const uint8_t *otid, lbm_transport_source_info_t *info, uint32_t infosize)
{
	MUL_FATAL_ASSERT(otid!=NULL);
	MUL_FATAL_ASSERT(info!=NULL);
	MUL_FATAL_ASSERT(sizeof(lbm_transport_source_info_t)==infosize);
	memset((void *) info, 0, infosize);
	switch (otid[LBM_OTID_TYPE_OFFSET]) {
		case LBM_TRANSPORT_TCP:
			return lbm_otid_tcp_to_transport_source(otid, info);
			break;
		case LBM_TRANSPORT_LBTRM:
			return lbm_otid_lbtrm_to_transport_source(otid, info);
			break;
		case LBM_TRANSPORT_LBTRU:
			return lbm_otid_lbtru_to_transport_source(otid, info);
			break;
		case LBM_TRANSPORT_LBTIPC:
			return lbm_otid_lbtipc_to_transport_source(otid, info);
			break;
		case LBM_TRANSPORT_LBTSMX:
			return lbm_otid_lbtsmx_to_transport_source(otid, info);
			break;
		case LBM_TRANSPORT_LBTRDMA:
			return lbm_otid_lbtrdma_to_transport_source(otid, info);
			break;
		default:
			return -1;
	}
}



const char *mul_inet_ntoa(uint32_t addr)
{
        struct in_addr iaddr;

        iaddr.s_addr = addr;
        return inet_ntoa(iaddr);
}




#define NULL_POINTER_SANITIZE(s) (s==NULL?"NULL Pointer":s)
#define TRANSPORT_STRING_UNKNOWN "unknown"
#define TRANSPORT_STRING_LBTRM "LBTRM"
#define TRANSPORT_STRING_LBTRU "LBT-RU"
#define TRANSPORT_STRING_TCP "TCP"
#define TRANSPORT_STRING_LBTIPC "LBT-IPC"
#define TRANSPORT_STRING_LBTSMX "LBT-SMX"
#define TRANSPORT_STRING_LBTRDMA "LBT-RDMA"


int lbm_transport_source_format(const lbm_transport_source_info_t *info, size_t infosize, char *source, size_t *size)
{
	char buf[LBM_MSG_MAX_SOURCE_LEN];
	char srcip[LBM_MSG_MAX_SOURCE_LEN];
	char mcgroup[LBM_MSG_MAX_SOURCE_LEN];

#if DEBUG
	printf("\n[lbm_transport_source_format] info[%p] source[%p] size[%u]\n", info, source, *size);
#endif
	if(source==NULL) {
		printf("ERROR lbm_transport_source_format: source must be valid");
		return (1);
	}
	if(info==NULL) {
		printf("ERROR: info must be valid");
		return (2);
	}
	if(sizeof(lbm_transport_source_info_t)>infosize) {
		printf("CoreApi-5688-4118: invalid info size");
		return (3);
	}

	switch (info->type) {
		case LBM_TRANSPORT_TYPE_LBTRM:
			snprintf(srcip, sizeof(srcip), "%s", mul_inet_ntoa(info->src_ip));
			snprintf(mcgroup, sizeof(mcgroup), "%s", mul_inet_ntoa(info->mc_group));
			if (info->topic_idx == 0) {
				snprintf(buf,
							sizeof(buf),
							TRANSPORT_STRING_LBTRM ":%s:%u:%x:%s:%u",
							srcip,
							ntohs(info->src_port),
							info->session_id,
							mcgroup,
							ntohs(info->dest_port));
			} else {
				snprintf(buf,
							sizeof(buf),
							TRANSPORT_STRING_LBTRM ":%s:%u:%x:%s:%u[%u]",
							srcip,
							ntohs(info->src_port),
							info->session_id,
							mcgroup,
							ntohs(info->dest_port),
							ntohl(info->topic_idx));
			}
			break;
		case LBM_TRANSPORT_TYPE_LBTRU:
			snprintf(srcip, sizeof(srcip), "%s", mul_inet_ntoa(info->src_ip));
			if (info->session_id == 0) {
				if (info->topic_idx == 0) {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_LBTRU ":%s:%u",
								srcip,
								ntohs(info->src_port));
				} else {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_LBTRU ":%s:%u[%u]",
								srcip,
								ntohs(info->src_port),
								ntohl(info->topic_idx));
				}
			} else {
				if (ntohl(info->topic_idx) == 0) {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_LBTRU ":%s:%u:%x",
								srcip,
								ntohs(info->src_port),
								info->session_id);
				} else {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_LBTRU ":%s:%u:%x[%u]",
								srcip,
								ntohs(info->src_port),
								info->session_id,
								ntohl(info->topic_idx));
				}
			}
			break;
		case LBM_TRANSPORT_TYPE_TCP:
			snprintf(srcip, sizeof(srcip), "%s", mul_inet_ntoa(info->src_ip));
			if (ntohl(info->session_id) == 0) {
				if (ntohl(info->topic_idx) == 0) {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_TCP ":%s:%u",
								srcip,
								ntohs(info->src_port));
				} else {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_TCP ":%s:%u[%u]",
								srcip,
								ntohs(info->src_port),
								ntohl(info->topic_idx));
				}
			}
			else {
				if (ntohl(info->topic_idx) == 0) {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_TCP ":%s:%u:%x",
								srcip,
								ntohs(info->src_port),
								ntohl(info->session_id));
				} else {
					snprintf(buf,
								sizeof(buf),
								TRANSPORT_STRING_TCP ":%s:%u:%x[%u]",
								srcip,
								ntohs(info->src_port),
								ntohl(info->session_id),
								ntohl(info->topic_idx));
				}
			}
			break;
		case LBM_TRANSPORT_TYPE_LBTIPC:
			if (ntohl(info->topic_idx) == 0) {
				snprintf(buf,
							sizeof(buf),
							TRANSPORT_STRING_LBTIPC ":%x:%u",
							info->session_id,
							info->transport_id);
			} else {
				snprintf(buf,
							sizeof(buf),
							TRANSPORT_STRING_LBTIPC ":%x:%u[%u]",
							info->session_id,
							info->transport_id,
							ntohl(info->topic_idx));
			}
			break;
		case LBM_TRANSPORT_TYPE_LBTSMX:
			if (ntohl(info->topic_idx) == 0) {
				snprintf(buf,
						sizeof(buf),
						TRANSPORT_STRING_LBTSMX ":%x:%u",
						info->session_id,
						info->transport_id);
			} else {
				snprintf(buf,
						sizeof(buf),
						TRANSPORT_STRING_LBTSMX ":%x:%u[%u]",
						info->session_id,
						info->transport_id,
						ntohl(info->topic_idx));
			}
			break;
		case LBM_TRANSPORT_TYPE_LBTRDMA:
			snprintf(srcip, sizeof(srcip), "%s", mul_inet_ntoa(info->src_ip));
			if (ntohl(info->topic_idx) == 0) {
				snprintf(buf,
							sizeof(buf),
							TRANSPORT_STRING_LBTRDMA ":%s:%u:%x",
							srcip,
							ntohs(info->src_port),
							info->session_id);
			} else {
				snprintf(buf,
							sizeof(buf),
							TRANSPORT_STRING_LBTRDMA ":%s:%u:%x[%u]",
							srcip,
							ntohs(info->src_port),
							ntohl(info->session_id),
							info->topic_idx);
			}
			break;
		default:
			snprintf(buf,
						 sizeof(buf),
						 TRANSPORT_STRING_UNKNOWN ":0x%x",
						 info->type);
			break;
	}
	if (strlen(buf) >= *size) {
		printf("\n ERROR! String buffer size not large enough strlen(buf)[%i] > *size[%i]", strlen(buf), *size);
		*size = strlen(buf) + 1;
		return 4;
	}
	*size = strlen(buf);
	strcpy(source, buf);
	return 0;
}

