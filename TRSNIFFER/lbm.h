/* 
 * lbm.h
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
#include <inttypes.h>

/* Types */
typedef unsigned int lbm_uint_t;
typedef unsigned long int lbm_ulong_t;
typedef unsigned short int lbm_ushort_t;
typedef unsigned char lbm_uchar_t;


typedef uint8_t lbm_uint8_t;
typedef uint16_t lbm_uint16_t;
typedef uint32_t lbm_uint32_t;
typedef uint64_t lbm_uint64_t;


#define LBM_CONTEXT_INSTANCE_BLOCK_SZ 8
#define LBM_TOPIC_RES_REQUEST_ADVERTISEMENT 0x04
#define LBM_TOPIC_RES_REQUEST_QUERY 0x02
#define LBM_TOPIC_RES_REQUEST_WILDCARD_QUERY 0x01
#define LBM_TOPIC_RES_REQUEST_CONTEXT_QUERY 0x20
#define LBM_TOPIC_RES_REQUEST_GW_REMOTE_INTEREST 0x40
#define LBM_OTID_BLOCK_SZ 32
#define LBM_CONTEXT_INSTANCE_BLOCK_SZ 8

/* Limits */
#define LBM_MSG_MAX_SOURCE_LEN 128
#define LBM_MSG_MAX_TOPIC_LEN 256
#define LBM_MSG_MAX_STATE_LEN 32
#define LBM_UME_MAX_STORE_STRLEN 24
#define LBM_UMQ_MAX_QUEUE_STRLEN 256
#define LBM_UMQ_MAX_TOPIC_STRLEN 256
#define LBM_UMQ_MAX_APPSET_STRLEN 256
#define LBM_MAX_CONTEXT_NAME_LEN 128
#define LBM_MAX_EVENT_QUEUE_NAME_LEN 128
#define LBM_MAX_UME_STORES 25
#define LBM_UMQ_ULB_MAX_RECEIVER_STRLEN 32
#define LBM_UMQ_MAX_INDEX_LEN 216
#define LBM_UMQ_USER_NAME_LENGTH_MAX 127
#define LBM_UMQ_PASSWORD_LENGTH_MAX 15
#define LBM_UMM_NUM_SERVERS_MAX 16
#define LBM_UMM_USER_NAME_LENGTH_MAX 100
#define LBM_UMM_APP_NAME_LENGTH_MAX 100
#define LBM_UMM_PASSWORD_LENGTH_MAX 100
#define LBM_UMM_SERVER_LENGTH_MAX 32
#define LBM_HMAC_BLOCK_SZ       20
#define LBM_MAX_GATEWAY_NAME_LEN 128
#define LBM_OTID_BLOCK_SZ 32
#define LBM_CONTEXT_INSTANCE_BLOCK_SZ 8


/* UM Transport Types */
/*! Transport type TCP. */
#define LBM_TRANSPORT_TYPE_TCP 0x00
/*! Transport type LBT-RU. */
#define LBM_TRANSPORT_TYPE_LBTRU 0x01
/*! Transport type LBT-SMX. */
#define LBM_TRANSPORT_TYPE_LBTSMX 0x4
/*! Transport type LBT-RM. */
#define LBM_TRANSPORT_TYPE_LBTRM 0x10
/*! Transport type LBT-IPC. */
#define LBM_TRANSPORT_TYPE_LBTIPC 0x40
/*! Transport type LBT-RDMA. */
#define LBM_TRANSPORT_TYPE_LBTRDMA 0x20

typedef struct lbm_transport_source_info_t_stct {
        /*! Type of transport. See LBM_TRANSPORT_TYPE_*. */
        int type;
        /*! Source IP address. Applicable only to LBT-RM, LBT-RU, TCP, and LBT-RDMA. Stored in network order. */
        lbm_uint32_t src_ip;
        /*! Source port. Applicable only to LBT-RM, LBT-RU, TCP, and LBT-RDMA. Stored in host order. */
        lbm_ushort_t src_port;
        /*! Destination port. Applicable only to LBT-RM. Stored in host order. */
        lbm_ushort_t dest_port;
        /*! Multicast group. Applicable only to LBT-RM. Stored in network order. */
        lbm_uint32_t mc_group;
        /*! Transport ID. Applicable only to LBT-IPC. Stored in host order. */
        lbm_uint32_t transport_id;
        /*! Session ID. Applicable to all transport. Stored in host order. */
        lbm_uint32_t session_id;
        /*! Topic index. Applicable to all transports. Stored in host order. */
        lbm_uint32_t topic_idx;
        /*! Transport index. Applicable to all transports. Stored in host order. */
        lbm_uint32_t transport_idx;
        char tname[LBM_MSG_MAX_TOPIC_LEN];
} lbm_transport_source_info_t;


