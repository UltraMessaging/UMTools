/* 
 * TS_lbmrpckt.h
 *
 * Created on: Jun 30, 2016
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
 *
 */
#ifndef LBM_RPCKT_H
#define LBM_RPCKT_H
#include "lbm.h"

typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t tqrs;
	lbm_uint16_t tirs;
} lbmr_hdr_t;
#define LBMR_HDR_SZ 4

#define LBMR_HDR_VER(x) (x >> 4)
#define LBMR_HDR_TYPE(x) (x & 0x7)

#define LBMR_HDR_TYPE_NORMAL 0x0
#define LBMR_HDR_TYPE_WC_TQRS 0x1
#define LBMR_HDR_TYPE_UCAST_RCV_ALIVE 0x2
#define LBMR_HDR_TYPE_UCAST_SRC_ALIVE 0x3
#define LBMR_HDR_TYPE_TOPIC_MGMT 0x4
#define LBMR_HDR_TYPE_QUEUE_RES 0x6
#define LBMR_HDR_TYPE_EXT 0x7
#define LBMR_HDR_TYPE_OPTS_MASK 0x8

typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t ext_type;
	lbm_uint16_t dep;
} lbmr_hdr_ext_type_t;
#define LBMR_HDR_EXT_TYPE_SZ 4

#define LBMR_HDR_EXT_TYPE_UME_PROXY_SRC_ELECT 0x1 /* Deprecated */
#define LBMR_HDR_EXT_TYPE_UMQ_QUEUE_MGMT 0x2
#define LBMR_HDR_EXT_TYPE_CONTEXT_INFO 0x3 /* Deprecated */
#define LBMR_HDR_EXT_TYPE_TOPIC_RES_REQUEST 0x4
#define LBMR_HDR_EXT_TYPE_TNWG_MSG 0x5
#define LBMR_HDR_EXT_TYPE_REMOTE_DOMAIN_ROUTE 0x6
#define LBMR_HDR_EXT_TYPE_REMOTE_CONTEXT_INFO 0x7
#define LBMR_HDR_EXT_TYPE_RANGE_LOW		LBMR_HDR_EXT_TYPE_UME_PROXY_SRC_ELECT
#define LBMR_HDR_EXT_TYPE_RANGE_HIGH	LBMR_HDR_EXT_TYPE_REMOTE_CONTEXT_INFO

typedef struct {
	lbm_uint8_t transport;
	lbm_uint8_t tlen;
	lbm_uint16_t ttl;
	lbm_uint32_t index; /* This is the topic index, not the transport index. */
} lbmr_tir_t;
#define LBMR_TIR_SZ 8

typedef struct {
	lbm_uint32_t ip;
	lbm_uint16_t port;
} lbmr_tir_tcp_t;
#define LBMR_TIR_TCP_SZ 6
typedef struct {
	lbm_uint32_t ip;
	lbm_uint32_t session_id;
	lbm_uint16_t port;
} lbmr_tir_tcp_with_sid_t;
#define LBMR_TIR_TCP_WITH_SID_SZ 10

typedef struct {
	lbm_uint32_t src_addr;
	lbm_uint32_t mcast_addr;
	lbm_uint32_t session_id;
	lbm_uint16_t udp_dest_port;
	lbm_uint16_t src_ucast_port;
} lbmr_tir_lbtrm_t;
#define LBMR_TIR_LBTRM_SZ sizeof(lbmr_tir_lbtrm_t)

typedef struct {
	lbm_uint32_t ip;
	lbm_uint16_t port;
} lbmr_tir_lbtru_t;
#define LBMR_TIR_LBTRU_SZ 6
typedef struct {
	lbm_uint32_t ip;
	lbm_uint32_t session_id;
	lbm_uint16_t port;
} lbmr_tir_lbtru_with_sid_t;
#define LBMR_TIR_LBTRU_WITH_SID_SZ 10

typedef struct {
	lbm_uint32_t host_id;
	lbm_uint32_t session_id;
	lbm_uint16_t xport_id;
} lbmr_tir_lbtipc_t;
#define LBMR_TIR_LBTIPC_SZ 10

typedef struct {
	lbm_uint32_t host_id;
	lbm_uint32_t session_id;
	lbm_uint16_t xport_id;
} lbmr_tir_lbtsmx_t;
#define LBMR_TIR_LBTSMX_SZ 10

typedef struct {
	lbm_uint32_t ip;
	lbm_uint32_t session_id;
	lbm_uint16_t port;
} lbmr_tir_lbtrdma_t;
#define LBMR_TIR_LBTRDMA_SZ 10

#define LBMR_TIR_TRANSPORT 0x7F
#define LBMR_TIR_OPTIONS 0x80

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
} lbmr_topic_opt_t;
#define LBMR_TOPIC_OPT_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_SZ 4

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t total_len;
} lbmr_topic_opt_len_t;
#define LBMR_TOPIC_OPT_LEN_TYPE 0x00
#define LBMR_TOPIC_OPT_LEN_SZ 4

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint16_t store_tcp_port;
	lbm_uint16_t src_tcp_port;
	lbm_uint32_t store_tcp_addr;
	lbm_uint32_t src_tcp_addr;
	lbm_uint32_t src_reg_id;
	lbm_uint32_t transport_idx;
	lbm_uint32_t high_seqnum;
	lbm_uint32_t low_seqnum;
} lbmr_topic_opt_ume_t;
#define LBMR_TOPIC_OPT_UME_TYPE 0x01
#define LBMR_TOPIC_OPT_UME_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_UME_FLAG_LATEJOIN 0x4000
#define LBMR_TOPIC_OPT_UME_FLAG_STORE 0x2000
#define LBMR_TOPIC_OPT_UME_FLAG_QCCAP 0x1000
#define LBMR_TOPIC_OPT_UME_FLAG_ACKTOSRC 0x800
#define LBMR_TOPIC_OPT_UME_FLAG_CTXID 0x400
#define LBMR_TOPIC_OPT_UME_SZ 32

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint8_t flags;
	lbm_uint8_t grp_idx;
	lbm_uint16_t store_tcp_port;
	lbm_uint16_t store_idx;
	lbm_uint32_t store_ip_addr;
	lbm_uint32_t src_reg_id;
} lbmr_topic_opt_ume_store_t;
#define LBMR_TOPIC_OPT_UME_STORE_TYPE 0x02
#define LBMR_TOPIC_OPT_UME_STORE_FLAG_IGNORE 0x80
#define LBMR_TOPIC_OPT_UME_STORE_FLAG_CTXID 0x40
#define LBMR_TOPIC_OPT_UME_STORE_SZ 16

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint8_t flags;
	lbm_uint8_t grp_idx;
	lbm_uint16_t grp_sz;
	lbm_uint16_t reserved;
} lbmr_topic_opt_ume_store_group_t;
#define LBMR_TOPIC_OPT_UME_STORE_GROUP_TYPE 0x03
#define LBMR_TOPIC_OPT_UME_STORE_GROUP_FLAG_IGNORE 0x80
#define LBMR_TOPIC_OPT_UME_STORE_GROUP_SZ 8

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint16_t src_tcp_port;
	lbm_uint16_t reserved;
	lbm_uint32_t src_ip_addr;
	lbm_uint32_t transport_idx;
	lbm_uint32_t high_seqnum;
	lbm_uint32_t low_seqnum;
} lbmr_topic_opt_latejoin_t;
#define LBMR_TOPIC_OPT_LATEJOIN_TYPE 0x04
#define LBMR_TOPIC_OPT_LATEJOIN_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_LATEJOIN_FLAG_ACKTOSRC 0x4000
#define LBMR_TOPIC_OPT_LATEJOIN_SZ 24

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t rcr_idx;
} lbmr_topic_opt_umq_rcridx_t;
#define LBMR_TOPIC_OPT_UMQ_RCRIDX_TYPE 0x05
#define LBMR_TOPIC_OPT_UMQ_RCRIDX_SZ 8
#define LBMR_TOPIC_OPT_UMQ_RCRIDX_FLAG_IGNORE 0x8000

#define LBMR_TOPIC_OPT_UMQ_QINFO_TYPE 0x06
#define LBMR_TOPIC_OPT_UMQ_FLAG_QUEUE 0x4000
#define LBMR_TOPIC_OPT_UMQ_FLAG_RCVLISTEN 0x2000
#define LBMR_TOPIC_OPT_UMQ_FLAG_CONTROL 0x1000
#define LBMR_TOPIC_OPT_UMQ_FLAG_SRCRCVLISTEN 0x0800
#define LBMR_TOPIC_OPT_UMQ_FLAG_PARTICIPANTS_ONLY 0x0400
#define LBMR_TOPIC_OPT_UMQ_MAX_QNAME_LEN 252

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint8_t flags;
	lbm_uint8_t hop_count;
	lbm_uint32_t cost;
} lbmr_topic_opt_cost_t;
#define LBMR_TOPIC_OPT_COST_TYPE 0x07
#define LBMR_TOPIC_OPT_COST_FLAG_IGNORE 0x80
#define LBMR_TOPIC_OPT_COST_SZ 8

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint8_t otid[LBM_OTID_BLOCK_SZ];
} lbmr_topic_opt_otid_t;
#define LBMR_TOPIC_OPT_OTID_TYPE 0x08
#define LBMR_TOPIC_OPT_OTID_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_OTID_SZ (LBM_OTID_BLOCK_SZ + 4)

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint8_t flags;
	lbm_uint8_t idx;
	lbm_uint8_t ctxinst[LBM_CONTEXT_INSTANCE_BLOCK_SZ];
} lbmr_topic_opt_ctxinst_t;
#define LBMR_TOPIC_OPT_CTXINST_TYPE 0x09
#define LBMR_TOPIC_OPT_CTXINST_FLAG_IGNORE 0x80
#define LBMR_TOPIC_OPT_CTXINST_SZ (LBM_CONTEXT_INSTANCE_BLOCK_SZ + 4)

/* CTXINSTS is identical to CTXINST, but applies to a store. */
#define LBMR_TOPIC_OPT_CTXINSTS_TYPE 0x0A
#define LBMR_TOPIC_OPT_CTXINSTS_FLAG_IGNORE 0x80
#define LBMR_TOPIC_OPT_CTXINSTS_SZ (LBM_CONTEXT_INSTANCE_BLOCK_SZ + 4)

/* CTXINSTQ is identical to CTXINST, but applies to a queue. */
#define LBMR_TOPIC_OPT_CTXINSTQ_TYPE 0x0C
#define LBMR_TOPIC_OPT_CTXINSTQ_FLAG_IGNORE 0x80
#define LBMR_TOPIC_OPT_CTXINSTQ_SZ (LBM_CONTEXT_INSTANCE_BLOCK_SZ + 4)

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t queue_id;
	lbm_uint8_t reg_id[8];
	lbm_uint32_t src_id;
	lbm_uint32_t src_ip_addr;
	lbm_uint16_t src_tcp_port;
	lbm_uint16_t reserved;
} lbmr_topic_opt_ulb_t;
#define LBMR_TOPIC_OPT_ULB_TYPE 0x0B
#define LBMR_TOPIC_OPT_ULB_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_ULB_SZ 28

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t domain_id;
} lbmr_topic_opt_domain_id_t;
#define LBMR_TOPIC_OPT_DOMAIN_ID_TYPE 0x0D
#define LBMR_TOPIC_OPT_DOMAIN_ID_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_DOMAIN_ID_SZ 8

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint16_t src_tcp_port;
	lbm_uint16_t reserved;
	lbm_uint32_t src_ip_addr;
	lbm_uint32_t functionality_flags;
} lbmr_topic_opt_exfunc_t;
#define LBMR_TOPIC_OPT_EXFUNC_TYPE 0x0E
#define LBMR_TOPIC_OPT_EXFUNC_FLAG_IGNORE 0x8000
#define LBMR_TOPIC_OPT_EXFUNC_SZ 16

#define LBMR_TOPIC_OPT_EXFUNC_FFLAG_LJ  0x00000001
#define LBMR_TOPIC_OPT_EXFUNC_FFLAG_UME 0x00000002
#define LBMR_TOPIC_OPT_EXFUNC_FFLAG_UMQ 0x00000004
#define LBMR_TOPIC_OPT_EXFUNC_FFLAG_ULB 0x00000008

/* Transports */
#define LBMR_TRANSPORT_TCP 0x00
#define LBMR_TRANSPORT_LBTRU 0x01
#define LBMR_TRANSPORT_TCP6 0x02
#define LBMR_TRANSPORT_LBTSMX 0x4
#define LBMR_TRANSPORT_LBTRM 0x10
#define LBMR_TRANSPORT_LBTRDMA 0x20
#define LBMR_TRANSPORT_LBTIPC 0x40

#define LBMR_TRANSPORT_PGM 0x11

#define LBMR_TRANSPORT_OPTION_MASK 0x80

typedef struct {
	lbm_uint16_t len;
	lbm_uint16_t tmrs;
} lbmr_tmb_t;
#define LBMR_TMB_SZ 4

typedef struct {
	lbm_uint16_t len;
	lbm_uint8_t type;
	lbm_uint8_t flags;
} lbmr_tmr_t;
#define LBMR_TMR_SZ 4

#define LBMR_TMR_LEAVE_TOPIC 0x00
#define LBMR_TMR_TOPIC_USE 0x01

#define LBMR_TMR_FLAG_RESPONSE 0x80
#define LBMR_TMR_FLAG_WILDCARD_PCRE 0x40
#define LBMR_TMR_FLAG_WILDCARD_REGEX 0x20
#define LBMR_TMR_FLAG_WILDCARD_MASK (LBMR_TMR_FLAG_WILDCARD_PCRE | LBMR_TMR_FLAG_WILDCARD_REGEX)

typedef struct {
	lbm_uint32_t queue_id;
	lbm_uint32_t queue_ver;
	lbm_uint32_t queue_prev_ver;
	lbm_uint16_t grp_blks;
	lbm_uint16_t queue_blks;
} lbmr_qir_t;
#define LBMR_QIR_SZ (16)

#define LBMR_QIR_OPTIONS 0x8000

typedef struct {
	lbm_uint16_t grp_idx;
	lbm_uint16_t grp_sz;
} lbmr_qir_grp_blk_t;
#define LBMR_QIR_GRP_BLK_SZ 4

typedef struct {
	lbm_uint32_t ip;
	lbm_uint16_t port;
	lbm_uint16_t idx;
	lbm_uint16_t grp_idx;
	lbm_uint16_t reserved;
} lbmr_qir_queue_blk_t;
#define LBMR_QIR_QUEUE_BLK_SZ (sizeof(lbmr_qir_queue_blk_t))
#define LBMR_QIR_QUEUE_BLK_FLAG_MASTER 0x8000

typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t ext_type;
	lbm_uint8_t len;
	lbm_uint8_t hop_count;
	lbm_uint16_t flags;
	lbm_uint16_t port;
	lbm_uint32_t ip;
	lbm_uint8_t instance[LBM_CONTEXT_INSTANCE_BLOCK_SZ];
} lbmr_ctxinfo_t;
#define LBMR_CTXINFO_BASE_SZ (sizeof(lbmr_ctxinfo_t))

#define LBMR_CTXINFO_QUERY_FLAG 0x8000
#define LBMR_CTXINFO_IP_FLAG 0x4000
#define LBMR_CTXINFO_INSTANCE_FLAG 0x2000
#define LBMR_CTXINFO_TNWG_SRC_FLAG 0x1000
#define LBMR_CTXINFO_TNWG_RCV_FLAG 0x0800
#define LBMR_CTXINFO_PROXY_FLAG 0x0400
#define LBMR_CTXINFO_NAME_FLAG 0x0001

typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t ext_type;
	lbm_uint16_t flags;
} lbmr_topic_res_request_t;
#define LBMR_TOPIC_RES_REQUEST_SZ (sizeof(lbmr_topic_res_request_t))

typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t ext_type;
	lbm_uint16_t num_domains;
	lbm_uint32_t ip;
	lbm_uint16_t port;
	lbm_uint16_t reserved;
	lbm_uint32_t length; /* in bytes */
	/* lbm_uint32_t domains[num_domains]; */
} lbmr_remote_domain_route_hdr_t;
/* Note that size is variable */
#define LBMR_REMOTE_DOMAIN_ROUTE_HDR_SZ (sizeof(lbmr_remote_domain_route_hdr_t))

typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t ext_type;
	lbm_uint16_t len;
	lbm_uint16_t type;
	lbm_uint16_t reserved;
} lbmr_tnwg_t;
#define LBMR_TNWG_BASE_SZ (sizeof(lbmr_tnwg_t))

#define LBMR_TNWG_TYPE_INTEREST 0x0000
#define LBMR_TNWG_TYPE_CTXINFO  0x0001
#define LBMR_TNWG_TYPE_TRREQ    0x0002

typedef struct {
	lbm_uint16_t len;
	lbm_uint16_t count;
} lbmr_tnwg_interest_t;
#define LBMR_TNWG_INTEREST_SZ (sizeof(lbmr_tnwg_interest_t))

typedef struct {
	lbm_uint16_t len;
	lbm_uint8_t flags;
	lbm_uint8_t pattype;
	lbm_uint32_t domain_id;
} lbmr_tnwg_interest_rec_t;
#define LBMR_TNWG_INTEREST_REC_BASE_SZ (sizeof(lbmr_tnwg_interest_rec_t))
#define LBMR_TNWG_INTEREST_REC_PATTERN_FLAG 0x80
#define LBMR_TNWG_INTEREST_REC_CANCEL_FLAG  0x40
#define LBMR_TNWG_INTEREST_REC_REFRESH_FLAG 0x20

typedef struct {
	lbm_uint16_t len;
	lbm_uint8_t hop_count;
	lbm_uint8_t reserved;
	lbm_uint32_t flags1;
	lbm_uint32_t flags2;
} lbmr_tnwg_ctxinfo_t;
#define LBMR_TNWG_CTXINFO_SZ (sizeof(lbmr_tnwg_ctxinfo_t))

#define LBMR_TNWG_CTXINFO_QUERY_FLAG 0x80000000
#define LBMR_TNWG_CTXINFO_TNWG_SRC_FLAG 0x40000000
#define LBMR_TNWG_CTXINFO_TNWG_RCV_FLAG 0x20000000
#define LBMR_TNWG_CTXINFO_PROXY_FLAG 0x10000000

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
} lbmr_tnwg_opt_t;
#define LBMR_TNWG_OPT_BASE_SZ (sizeof(lbmr_tnwg_opt_t))
#define LBMR_TNWG_OPT_IGNORE_FLAG 0x8000

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint8_t instance[LBM_CONTEXT_INSTANCE_BLOCK_SZ];
} lbmr_tnwg_opt_ctxinst_t;
#define LBMR_TNWG_OPT_CTXINST_SZ (sizeof(lbmr_tnwg_opt_ctxinst_t))
#define LBMR_TNWG_OPT_CTXINST_TYPE 0x00

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint16_t port;
	lbm_uint16_t res;
	lbm_uint32_t ip;
} lbmr_tnwg_opt_address_t;
#define LBMR_TNWG_OPT_ADDRESS_SZ (sizeof(lbmr_tnwg_opt_address_t))
#define LBMR_TNWG_OPT_ADDRESS_TYPE 0x01

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t domain_id;
} lbmr_tnwg_opt_domain_t;
#define LBMR_TNWG_OPT_DOMAIN_SZ (sizeof(lbmr_tnwg_opt_domain_t))
#define LBMR_TNWG_OPT_DOMAIN_TYPE 0x02

#define LBMR_TNWG_OPT_NAME_BASE_SZ (sizeof(lbmr_tnwg_opt_t))
#define LBMR_TNWG_OPT_NAME_TYPE 0x03

typedef struct {
	lbm_uint16_t len;
} lbmr_tnwg_trreq_t;
#define LBMR_TNWG_TRREQ_SZ (sizeof(lbmr_tnwg_trreq_t))


typedef struct {
	lbm_uint8_t ver_type;
	lbm_uint8_t ext_type;
	lbm_uint16_t len;
	lbm_uint16_t num_recs;
	lbm_uint16_t reserved;
} lbmr_rctxinfo_t;
#define LBMR_RCTXINFO_SZ (sizeof(lbmr_rctxinfo_t))

typedef struct {
	lbm_uint16_t len;
	lbm_uint16_t flags;
} lbmr_rctxinfo_rec_t;
#define LBM_RCTXINFO_REC_SZ (sizeof(lbmr_rctxinfo_rec_t))

#define LBMR_RCTXINFO_REC_FLAG_QUERY 0x8000

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
} lbmr_rctxinfo_rec_opt_t;
#define LBMR_RCTXINFO_REC_OPT_SZ (sizeof(lbmr_rctxinfo_rec_opt_t))

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t domain_id;
	lbm_uint32_t ip;
	lbm_uint16_t port;
	lbm_uint16_t res;
} lbmr_rctxinfo_rec_address_opt_t;
#define LBMR_RCTXINFO_REC_ADDRESS_OPT_SZ (sizeof(lbmr_rctxinfo_rec_address_opt_t))

#define LBMR_RCTXINFO_OPT_ADDRESS_TYPE 0x01

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint8_t instance[LBM_CONTEXT_INSTANCE_BLOCK_SZ];
} lbmr_rctxinfo_rec_instance_opt_t;
#define LBMR_RCTXINFO_REC_INSTANCE_OPT_SZ (sizeof(lbmr_rctxinfo_rec_instance_opt_t))

#define LBMR_RCTXINFO_OPT_INSTANCE_TYPE 0x02

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t domain_id;
} lbmr_rctxinfo_rec_odomain_opt_t;
#define LBMR_RCTXINFO_REC_ODOMAIN_OPT_SZ (sizeof(lbmr_rctxinfo_rec_domain_opt_t))

#define LBMR_RCTXINFO_OPT_ODOMAIN_TYPE 0x03

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
} lbmr_rctxinfo_rec_name_opt_t;
#define LBMR_RCTXINFO_REC_NAME_OPT_BASE_SZ (sizeof(lbmr_rctxinfo_rec_name_opt_t))

#define LBMR_RCTXINFO_OPT_NAME_TYPE 0x04


typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
} lbmr_lbmr_opt_hdr_t;
#define LBMR_LBMR_OPT_HDR_SZ sizeof(lbmr_lbmr_opt_hdr_t)
#define LBMR_LBMR_OPT_HDR_FLAG_IGNORE 0x8000

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t total_len;
} lbmr_lbmr_opt_len_t;
#define LBMR_LBMR_OPT_LEN_TYPE 0x80
#define LBMR_LBMR_OPT_LEN_SZ sizeof(lbmr_lbmr_opt_len_t)

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	char src_id[8];
} lbmr_lbmr_opt_src_id_t;
#define LBMR_LBMR_OPT_SRC_ID_TYPE 0x81
#define LBMR_LBMR_OPT_SRC_ID_SZ sizeof(lbmr_lbmr_opt_src_id_t)
#define LBMR_LBMR_OPT_SRC_ID_FLAG_IGNORE 0x8000

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint8_t flags;
	lbm_uint8_t src_type;
} lbmr_lbmr_opt_src_type_t;
#define LBMR_LBMR_OPT_SRC_TYPE_TYPE 0x82
#define LBMR_LBMR_OPT_SRC_TYPE_SZ sizeof(lbmr_lbmr_opt_src_type_t)
#define LBMR_LBMR_OPT_SRC_TYPE_FLAG_IGNORE 0x80

#define LBMR_LBMR_OPT_SRC_TYPE_SRC_TYPE_APPLICATION 0
#define LBMR_LBMR_OPT_SRC_TYPE_SRC_TYPE_TNWGD 1
#define LBMR_LBMR_OPT_SRC_TYPE_SRC_TYPE_STORE 2

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t version;
} lbmr_lbmr_opt_version_t;
#define LBMR_LBMR_OPT_VERSION_TYPE 0x83
#define LBMR_LBMR_OPT_VERSION_SZ sizeof(lbmr_lbmr_opt_version_t)

#define LBMR_LBMR_OPT_VERSION_FLAG_IGNORE		0x8000
#define LBMR_LBMR_OPT_VERSION_FLAG_UME			0x0001
#define LBMR_LBMR_OPT_VERSION_FLAG_UMQ			0x0002

#define LBMR_VERSION_0 0x00
#define LBMR_VERSION_1 0x01
#define LBMR_VERSION_GATEWAY LBMR_VERSION_1
#define LBMR_VERSION LBMR_VERSION_1

typedef struct {
	lbm_uint8_t type;
	lbm_uint8_t len;
	lbm_uint16_t flags;
	lbm_uint32_t local_domain_id;
} lbmr_lbmr_opt_local_domain_t;
#define LBMR_LBMR_OPT_LOCAL_DOMAIN_TYPE 0x84
#define LBMR_LBMR_OPT_LOCAL_DOMAIN_SZ sizeof(lbmr_lbmr_opt_local_domain_t)
#define LBMR_LBMR_OPT_LOCAL_DOMAIN_FLAG_IGNORE		0x8000


struct lbm_lbmr_options_t_stct {
        lbm_uint32_t flags;
        lbm_uint8_t src_id[LBM_CONTEXT_INSTANCE_BLOCK_SZ];
        lbm_uint8_t app_type;
        struct {
                lbm_uint8_t flags;
                lbm_uint32_t version;
        } version;
};

#define LBM_MAX_TOPIC_NAME_LEN 256
#endif /* LBM_INTERNAL_USE_ONLY */



#ifndef LBM_TOPIC_H
/* Choice of transports */
#define LBM_TRANSPORT_TCP 0x00
#define LBM_TRANSPORT_LBTRU 0x01
#define LBM_TRANSPORT_TCP6 0x02
#define LBM_TRANSPORT_LBTSMX 0x4
#define LBM_TRANSPORT_LBTRM 0x10
#define LBM_TRANSPORT_LBTRDMA 0x20
#define LBM_TRANSPORT_LBTIPC 0x40
#define LBM_TRANSPORT_PGM 0x11
/* we save one as a flag to indicate usage of the daemon in the imbq */
#define LBM_TRANSPORT_DAEMON 0xFF


#define MUL_FATAL_ASSERT(exp) ((exp)?(void )0:printf("failed assertion [%s] at line %d in %s\n ", #exp,__FILE__,__LINE__))


#endif
