/**     \file tnwgdmonmsgs.h
        \brief Ultra Messaging (UM) message definitions for UM Router (DRO)
        Daemon Statistics.
        For general information on Daemon Statistics, see \ref storedaemonstatistics.

  All of the documentation and software included in this and any
  other Informatica Corporation Ultra Messaging Releases
  Copyright (C) Informatica Corporation. All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted only as covered by the terms of a
  valid software license agreement with Informatica Corporation.

  Copyright (C) 2004-2017, Informatica Corporation. All Rights Reserved.

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

#ifndef TNWG_DMON_MSGS_H
#define TNWG_DMON_MSGS_H

/*! \brief Value of tnwg_dstat_msg_hdr_t_stct::version indicating
	the version of the monitoring daemon.  See \ref daemonstatisticsversioning
	for general information on versioning of these structures. */
#define LBM_TNWG_DMON_VERSION 0

/*! \brief Common message header structure included at the start of all
	messages.  Used by \ref tnwg_dstat_mallinfo_msg_t,
	\ref tnwg_dstat_portalstats_msg_t, \ref tnwg_dstat_gatewaycfg_msg_t
	\ref tnwg_pcfg_stat_grp_msg_t, \ref tnwg_rm_stat_grp_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_msg_hdr_t_stct {
	/*! \brief "Magic" value set by sender to indicate to the receiver
		whether byte swapping is needed.
		Possible values: \ref LBM_TNWG_DAEMON_MAGIC,
		\ref LBM_TNWG_DAEMON_ANTIMAGIC. */
	lbm_uint16_t magic;
	/*! \brief Message type set by sender to indicate which kind of message
		this is.
		Possible values: one of the TNWG_DSTATTYPE_* constants
		( \ref TNWG_DSTATTYPE_PORTSTAT,
		\ref TNWG_DSTATTYPE_GATEWAYCFG, etc. ) */
	lbm_uint16_t type;
	/*! \brief Version of the message definition.  See
		\ref daemonstatisticsversioning for general information on
		versioning of these structures. */
	lbm_uint16_t version;
	/*! \brief Total length of the message, including this header.
		Note that some message types do not have fixed lengths. */
	lbm_uint16_t length;
	/*! \brief Approximate timestamp when the message was sent.
		Represents local wall clock time from the sending host's perspective.
		Value is "POSIX Time" (seconds since 1-Jan-1970),
		but in sending host's timezone. */
	lbm_uint32_t tv_sec;
	/*! \brief Count of microseconds to be added to "tv_sec" to increase
		the precision of the timestamp.  However, the accuracy of the
		timestamp is not guaranteed to be at the microsecond level. */
	lbm_uint32_t tv_usec;
} tnwg_dstat_msg_hdr_t;



#define TNWG_DSTAT_FILENAME_MAX_STRLEN 256
#define TNWG_DSTAT_MAX_PORTAL_NAME_LEN 256
#define TNWG_DSTAT_MAX_GATEWAY_NAME_LEN 256


/*! \brief For internal use only. */
#define	TNWG_DSTATTYPE_INVALID 0
/*! \brief For internal use only. */
#define	TNWG_DSTATTYPE_RM 1

/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	Local Route Information message, of type:
	\ref tnwg_rm_stat_grp_msg_t.  For this message type, the union field
	\ref tnwg_rm_stat_grp_msg_t_stct::msgtype.`local` should be used.  */
#define	TNWG_DSTATTYPE_RM_LOCAL 2
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	Portal Route Information message, of type:
	\ref tnwg_rm_stat_grp_msg_t.  For this message type, the union field
	\ref tnwg_rm_stat_grp_msg_t_stct::msgtype.`portal` should be used.  */
#define	TNWG_DSTATTYPE_RM_PORTAL 3
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for an
	Other DRO Route Information message, of type:
	\ref tnwg_rm_stat_grp_msg_t.  For this message type, the union field
	\ref tnwg_rm_stat_grp_msg_t_stct::msgtype.`othergw` should be used.  */
#define	TNWG_DSTATTYPE_RM_OTHERGW 4
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	Neighbor DRO Route Information message, of type:
	\ref tnwg_rm_stat_grp_msg_t.  For this message type, the union field
	\ref tnwg_rm_stat_grp_msg_t_stct::msgtype.`neighbor` should be used.  */
#define	TNWG_DSTATTYPE_RM_OTHERGW_NBR 5
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	Portal Statistics message, of type:
	\ref tnwg_dstat_portalstats_msg_t. */
#define	TNWG_DSTATTYPE_PORTSTAT 6
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	DRO Configuration message, of type:
	\ref tnwg_dstat_gatewaycfg_msg_t. */
#define	TNWG_DSTATTYPE_GATEWAYCFG 7
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	Portal Configuration message, of type:
	\ref tnwg_pcfg_stat_grp_msg_t. */
#define	TNWG_DSTATTYPE_PORTCFG 8
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::type for a
	Memory Allocation Statistics message, of type:
	\ref tnwg_dstat_mallinfo_msg_t. */
#define	TNWG_DSTATTYPE_MALLINFO 9


/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::magic indicating that
	the message does NOT need byte swapping. */
#define LBM_TNWG_DAEMON_MAGIC 0x4542
/*! \brief Value of \ref tnwg_dstat_msg_hdr_t_stct::magic indicating that
        the message DOES need byte swapping. */
#define LBM_TNWG_DAEMON_ANTIMAGIC 0x4245


/*! \brief Value of \ref tnwg_dstat_record_hdr_t_stct::portal_type
	and \ref tnwg_pcfg_stat_grp_rec_hdr_t_stct::portal_type for
	endpoint portals. */
#define	TNWG_DSTAT_Portal_Type_Endpoint 0
/*! \brief Value of \ref tnwg_dstat_record_hdr_t_stct::portal_type
	and \ref tnwg_pcfg_stat_grp_rec_hdr_t_stct::portal_type for
	peer portals. */
#define TNWG_DSTAT_Portal_Type_Peer 1

/*! \brief value of \ref tnwg_rm_stat_grp_othergw_neighbor_t::domain_or_gateway
	for domain-type neighbor route information. */
#define TNWG_DSTAT_Domain_Type 0
/*! \brief value of \ref tnwg_rm_stat_grp_othergw_neighbor_t::domain_or_gateway
	for DRO-type neighbor route information. */
#define TNWG_DSTAT_Gateway_Type 1


/*! \brief Structure containing memory statistics.  Used by
	\ref tnwg_dstat_mallinfo_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_mallinfo_stat_grp_record_t_stct {
	/*! \brief Non-mmapped space allocated (bytes). */
	lbm_uint32_t arena;
	/*! \brief Number of free chunks. */
	lbm_uint32_t ordblks;
	/*! \brief Number of mmapped regions. */
	lbm_uint32_t hblks;
	/*! \brief Space allocated in mmapped regions (bytes). */
	lbm_uint32_t hblkhd;
	/*! \brief Total allocated space (bytes). */
	lbm_uint32_t uordblks;
	/*! \brief Total free space (bytes). */
	lbm_uint32_t fordblks;
} tnwg_mallinfo_stat_grp_record_t;

/*! \brief Message containing memory allocation statistics.
	<br>( \ref tnwg_dstat_msg_hdr_t_stct::type == \ref TNWG_DSTATTYPE_MALLINFO )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_mallinfo_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	tnwg_dstat_msg_hdr_t hdr;
	/*! \brief Memory statistics. */
	tnwg_mallinfo_stat_grp_record_t record;
} tnwg_dstat_mallinfo_msg_t;


/*! \brief Structure containing receiving statistics for an endpoint portal.
	Used by \ref tnwg_portal_endpoint_dstat_record_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_endpoint_receive_stats_t_stct
{
	/*! \brief Transport topic message fragments received. */
	uint64_t transport_topic_fragments_rcvd;
	/*! \brief Transport topic message fragment bytes received. */
	uint64_t transport_topic_fragment_bytes_rcvd;
	/*! \brief Transport topic message request fragments received. */
	uint64_t transport_topic_req_fragments_rcvd;
	/*! \brief Transport topic message request fragment bytes received. */
	uint64_t transport_topic_req_fragment_bytes_rcvd;
	
	/*! \brief Transport topic control message received. */
	uint64_t transport_topic_control_rcvd;
	/*! \brief Transport topic control message bytes received. */
	uint64_t transport_topic_control_bytes_rcvd;

	/*! \brief Immediate topic message fragments received. */
	uint64_t immediate_topic_fragments_rcvd;
	/*! \brief Immediate topic message fragment bytes received. */
	uint64_t immediate_topic_fragment_bytes_rcvd;
	/*! \brief Immediate topic message request fragments received. */
	uint64_t immediate_topic_req_fragments_rcvd;
	/*! \brief Immediate topic message request fragment bytes received. */
	uint64_t immediate_topic_req_fragment_bytes_rcvd;

	/*! \brief Immediate topicless message fragments received. */
	uint64_t immediate_topicless_fragments_rcvd;
	/*! \brief Immediate topicless message fragment bytes received. */
	uint64_t immediate_topicless_fragment_bytes_rcvd;
	/*! \brief Immediate topicless message request fragments received. */
	uint64_t immediate_topicless_req_fragments_rcvd;
	/*! \brief Immediate topicless message request fragment bytes received. */
	uint64_t immediate_topicless_req_fragment_bytes_rcvd;

	/*! \brief Unicast data messages received. */
	uint64_t unicast_data_msgs_rcvd;
	/*! \brief Unicast data message bytes received. */
	uint64_t unicast_data_msg_bytes_rcvd;
	/*! \brief Unicast data messages received with no stream identification. */
	uint64_t unicast_data_msgs_rcvd_no_stream;
	/*! \brief Unicast data message bytes received with no stream identification. */
	uint64_t unicast_data_msg_bytes_rcvd_no_stream;
	/*! \brief Unicast data messages dropped as duplicates. */
	uint64_t unicast_data_msgs_dropped_dup;
	/*! \brief Unicast data message bytes dropped as duplicates. */
	uint64_t unicast_data_msg_bytes_dropped_dup;
	/*! \brief Unicast data messages dropped no route */
	uint64_t unicast_data_msgs_dropped_no_route;
	/*! \brief Unicast data message bytes dropped no route */
	uint64_t unicast_data_msg_bytes_dropped_no_route;

	/*! \brief Unicast control messages received. */
	uint64_t unicast_cntl_msgs_rcvd;
	/*! \brief Unicast control message bytes received. */
	uint64_t unicast_cntl_msg_bytes_rcvd;
	/*! \brief Unicast control messages received with no stream identification. */
	uint64_t unicast_cntl_msgs_rcvd_no_stream;
	/*! \brief Unicast control message bytes received with no stream identification. */
	uint64_t unicast_cntl_msg_bytes_rcvd_no_stream;
	/*! \brief Unicast control messages dropped as duplicates. */
	uint64_t unicast_cntl_msgs_dropped_dup;
	/*! \brief Unicast control message bytes dropped as duplicates. */
	uint64_t unicast_cntl_msg_bytes_dropped_dup;
	/*! \brief Unicast control messages dropped no route */
	uint64_t unicast_cntl_msgs_dropped_no_route;
	/*! \brief Unicast control message bytes dropped no route */
	uint64_t unicast_cntl_msg_bytes_dropped_no_route;
} tnwg_dstat_endpoint_receive_stats_t;


/*! \brief Structure containing sending statistics for an endpoint portal.
	Used by \ref tnwg_portal_endpoint_dstat_record_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_endpoint_send_stats_t_stct
{
	/*! \brief Transport topic fragments forwarded to this portal. */
	uint64_t transport_topic_fragments_forwarded;
	/*! \brief Transport topic fragment bytes forwarded to this portal. */
	uint64_t transport_topic_fragment_bytes_forwarded;
	/*! \brief Transport topic fragments sent. */
	uint64_t transport_topic_fragments_sent;
	/*! \brief Transport topic fragment bytes sent. */
	uint64_t transport_topic_fragment_bytes_sent;
	/*! \brief Transport topic request fragments sent. */
	uint64_t transport_topic_req_fragments_sent;
	/*! \brief Transport topic request fragment bytes sent. */
	uint64_t transport_topic_req_fragment_bytes_sent;
	/*! \brief Duplicate transport topic fragments dropped. */
	uint64_t transport_topic_fragments_dropped_dup;
	/*! \brief Duplicate transport topic fragment bytes dropped. */
	uint64_t transport_topic_fragment_bytes_dropped_dup;
	/*! \brief Transport topic fragments dropped due to EWOULDBLOCK. */
	uint64_t transport_topic_fragments_dropped_would_block;
	/*! \brief Transport topic fragment bytes dropped due to EWOULDBLOCK. */
	uint64_t transport_topic_fragment_bytes_dropped_would_block;
	/*! \brief Transport topic fragments dropped due to error. */
	uint64_t transport_topic_fragments_dropped_error;
	/*! \brief Transport topic fragment bytes dropped due to error. */
	uint64_t transport_topic_fragment_bytes_dropped_error;
	/*! \brief Transport topic fragments dropped due to fragment size error. */
	uint64_t transport_topic_fragments_dropped_size_error;
	/*! \brief Transport topic fragment dropped due to fragment size error. */
	uint64_t transport_topic_fragment_bytes_dropped_size_error;
	
	/*! \brief Immediate topic fragments forwarded. */
	uint64_t immediate_topic_fragments_forwarded;
	/*! \brief Immediate topic fragment bytes forwarded. */
	uint64_t immediate_topic_fragment_bytes_forwarded;
	/*! \brief Immediate topic fragments sent. */
	uint64_t immediate_topic_fragments_sent;
	/*! \brief Immediate topic fragment bytes sent. */
	uint64_t immediate_topic_fragment_bytes_sent;
	/*! \brief Immediate topic request fragments sent. */
	uint64_t immediate_topic_req_fragments_sent;
	/*! \brief Immediate topic request fragment bytes sent. */
	uint64_t immediate_topic_req_fragment_bytes_sent;
	/*! \brief Immediate topic fragments dropped due to EWOULDBLOCK. */
	uint64_t immediate_topic_fragments_dropped_would_block;
	/*! \brief Immediate topic fragment bytes dropped due to EWOULDBLOCK. */
	uint64_t immediate_topic_fragment_bytes_dropped_would_block;
	/*! \brief Immediate topic fragments dropped due to error. */
	uint64_t immediate_topic_fragments_dropped_error;
	/*! \brief Immediate topic fragment bytes dropped due to error. */
	uint64_t immediate_topic_fragment_bytes_dropped_error;
	/*! \brief Immediate topic fragments dropped due to fragment size error. */
	uint64_t immediate_topic_fragments_dropped_size_error;
	/*! \brief Immediate topic fragment bytes dropped due to fragment size error. */
	uint64_t immediate_topic_fragment_bytes_dropped_size_error;
	
	/*! \brief Immediate topicless fragments forwarded. */
	uint64_t immediate_topicless_fragments_forwarded;
	/*! \brief Immediate topicless fragment bytes forwarded. */
	uint64_t immediate_topicless_fragment_bytes_forwarded;
	/*! \brief Immediate topicless fragments sent. */
	uint64_t immediate_topicless_fragments_sent;
	/*! \brief Immediate topicless fragment bytes sent. */
	uint64_t immediate_topicless_fragment_bytes_sent;
	/*! \brief Immediate topicless request fragments sent. */
	uint64_t immediate_topicless_req_fragments_sent;
	/*! \brief Immediate topicless request fragment bytes sent. */
	uint64_t immediate_topicless_req_fragment_bytes_sent;
	/*! \brief Immediate topicless fragments dropped due to EWOULDBLOCK. */
	uint64_t immediate_topicless_fragments_dropped_would_block;
	/*! \brief Immediate topicless fragment bytes dropped due to EWOULDBLOCK. */
	uint64_t immediate_topicless_fragment_bytes_dropped_would_block;
	/*! \brief Immediate topicless fragments dropped due to error. */
	uint64_t immediate_topicless_fragments_dropped_error;
	/*! \brief Immediate topicless fragment bytes dropped due to error. */
	uint64_t immediate_topicless_fragment_bytes_dropped_error;
	/*! \brief Immediate topicless fragments dropped due to fragment size error. */
	uint64_t immediate_topicless_fragments_dropped_size_error;
	/*! \brief Immediate topicless fragment bytes dropped due to fragment size error. */
	uint64_t immediate_topicless_fragment_bytes_dropped_size_error;

	/*! \brief Unicast messages forwarded. */
	uint64_t unicast_msgs_forwarded;
	/*! \brief Unicast message bytes forwarded. */
	uint64_t unicast_msg_bytes_forwarded;
	/*! \brief Unicast messages sent. */
	uint64_t unicast_msgs_sent;
	/*! \brief Unicast message bytes sent. */
	uint64_t unicast_msg_bytes_sent;
	/*! \brief Unicast messages dropped due to error. */
	uint64_t unicast_msgs_dropped_error;
	/*! \brief Unicast message bytes dropped due to error. */
	uint64_t unicast_msg_bytes_dropped_error;
	
	/*! \brief Current data bytes enqueued internally. */
	uint64_t data_bytes_enqueued;
	/*! \brief Maximum data bytes enqueued internally. */
	uint64_t data_bytes_enqueued_max;
	/*! \brief Configured maximum data bytes allowed in queued. */
	uint64_t data_bytes_enqueued_limit;
} tnwg_dstat_endpoint_send_stats_t;


/*! \brief Structure containing receiving statistics for a peer portal.
	Used by \ref tnwg_portal_peer_dstat_record_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_peer_receive_stats_t_stct
{
	/*! \brief Data messages received. */
	uint64_t data_msgs_rcvd;
	/*! \brief Date message bytes received. */
	uint64_t data_msg_bytes_rcvd;

	/*! \brief Transport topic fragment data messages received.*/
	uint64_t transport_topic_fragment_data_msgs_rcvd;
	/*! \brief Transport topic fragment data message bytes received.*/
	uint64_t transport_topic_fragment_data_msg_bytes_rcvd;
	/*! \brief Transport topic fragment data messages received with an unknown source. */
	uint64_t transport_topic_fragment_data_msgs_rcvd_unknown_source;
	/*! \brief Transport topic fragment data message bytes received with an unknown source. */
	uint64_t transport_topic_fragment_data_msg_bytes_rcvd_unknown_source;
	/*! \brief Transport topic request fragment data messages received. */
	uint64_t transport_topic_req_fragment_data_msgs_rcvd;
	/*! \brief Transport topic request fragment data message bytes received. */
	uint64_t transport_topic_req_fragment_data_msg_bytes_rcvd;
	/*! \brief Transport topic request fragment data messages received with an unknown source. */
	uint64_t transport_topic_req_fragment_data_msgs_rcvd_unknown_source;
	/*! \brief Transport topic request fragment data message bytes received with an unknown source. */
	uint64_t transport_topic_req_fragment_data_msg_bytes_rcvd_unknown_source;

	/*! \brief Transport topic control messages received.*/
	uint64_t transport_topic_control_msgs_rcvd;
	/*! \brief Transport topic control message bytes received.*/
	uint64_t transport_topic_control_msg_bytes_rcvd;
	/*! \brief Transport topic control messages received with an unknown source. */
	uint64_t transport_topic_control_msgs_rcvd_unknown_source;
	/*! \brief Transport topic control message bytes received with an unknown source. */
	uint64_t transport_topic_control_msg_bytes_rcvd_unknown_source;
	
	/*! \brief Immediate topic fragment data messages received. */
	uint64_t immediate_topic_fragment_data_msgs_rcvd;
	/*! \brief Immediate topic fragment data message bytes received. */
	uint64_t immediate_topic_fragment_data_msg_bytes_rcvd;
	/*! \brief Immediate topic request fragment data messages received. */
	uint64_t immediate_topic_req_fragment_data_msgs_rcvd;
	/*! \brief Immediate topic request fragment data message bytes received. */
	uint64_t immediate_topic_req_fragment_data_msg_bytes_rcvd;
	
	/*! \brief Immediate topicless fragment data messages received. */
	uint64_t immediate_topicless_fragment_data_msgs_rcvd;
	/*! \brief Immediate topicless fragment data message bytes received. */
	uint64_t immediate_topicless_fragment_data_msg_bytes_rcvd;
	/*! \brief Immediate topicless request fragment data messages received. */
	uint64_t immediate_topicless_req_fragment_data_msgs_rcvd;
	/*! \brief Immediate topicless request fragment data message bytes received. */
	uint64_t immediate_topicless_req_fragment_data_msg_bytes_rcvd;
	
	/*! \brief Unicast data messages received. */
	uint64_t unicast_data_msgs_rcvd;
	/*! \brief Unicast data message bytes received. */
	uint64_t unicast_data_msg_bytes_rcvd;
	/*! \brief Unicast data messages received with no forwarding information. */
	uint64_t unicast_data_msgs_rcvd_no_fwd;
	/*! \brief Unicast data message bytes received with no forwarding information. */
	uint64_t unicast_data_msg_bytes_rcvd_no_fwd;
	/*! \brief Unicast data messages received with unknown forwarding information. */
	uint64_t unicast_data_msgs_rcvd_unknown_fwd;
	/*! \brief Unicast data message bytes received with unknown forwarding information. */
	uint64_t unicast_data_msg_bytes_rcvd_unknown_fwd;
	/*! \brief Unicast data messages received with no stream information. */
	uint64_t unicast_data_msgs_rcvd_no_stream;
	/*! \brief Unicast data message bytes received with no stream information. */
	uint64_t unicast_data_msg_bytes_rcvd_no_stream;
	/*! \brief Unicast data messages dropped no route */
	uint64_t unicast_data_msgs_dropped_no_route;
	/*! \brief Unicast data message bytes dropped no route */
	uint64_t unicast_data_msg_bytes_dropped_no_route;

	/*! \brief Control messages received. */
	uint64_t cntl_msgs_rcvd;
	/*! \brief Date message bytes received. */
	uint64_t cntl_msg_bytes_rcvd;

	/*! \brief Unicast control messages received. */
	uint64_t unicast_cntl_msgs_rcvd;
	/*! \brief Control message bytes received. */
	uint64_t unicast_cntl_msg_bytes_rcvd;
	/*! \brief Retransmission requests received. */
	uint64_t unicast_cntl_rxreq_msgs_rcvd;
	/*! \brief Retransmission request bytes received. */
	uint64_t unicast_cntl_rxreq_msg_bytes_rcvd;
	/*! \brief Unicast control messages received but unhandled. */
	uint64_t unicast_cntl_msgs_rcvd_unhandled;
	/*! \brief Unicast control message bytes received but unhandled. */
	uint64_t unicast_cntl_msg_bytes_rcvd_unhandled;
	/*! \brief Unicast control messages received with no stream information. */
	uint64_t unicast_cntl_msgs_rcvd_no_stream;
	/*! \brief Unicast control message bytes received with no stream information. */
	uint64_t unicast_cntl_msg_bytes_rcvd_no_stream;
	/*! \brief Unicast control messages dropped no route */
	uint64_t unicast_cntl_msgs_dropped_no_route;
	/*! \brief Unicast control message bytes dropped no route */
	uint64_t unicast_cntl_msg_bytes_dropped_no_route;

	/*! \brief DRO control messages received. */
	uint64_t gateway_cntl_msgs_rcvd;
	/*! \brief DRO control message bytes received. */
	uint64_t gateway_cntl_msg_bytes_rcvd;
} tnwg_dstat_peer_receive_stats_t;


/*! \brief Structure containing sending statistics for a peer portal.
	Used by \ref tnwg_portal_peer_dstat_record_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_peer_send_stats_t_stct
{
	/*! \brief Data fragments forwarded to this portal. */
	uint64_t data_fragments_forwarded;
	/*! \brief Data fragment bytes forwarded to this portal. */
	uint64_t data_fragment_bytes_forwarded;
	/*! \brief Data fragments sent. */
	uint64_t data_fragments_sent;
	/*! \brief Data fragment bytes sent. */
	uint64_t data_fragment_bytes_sent;
	/*! \brief Duplicate data fragments dropped. */
	uint64_t data_fragments_dropped_dup;
	/*! \brief Duplicate data fragment bytes dropped. */
	uint64_t data_fragment_bytes_dropped_dup;
	/*! \brief Data fragments dropped due to EWOULDBLOCK. */
	uint64_t data_fragments_dropped_would_block;
	/*! \brief Data fragment bytes dropped due to EWOULDBLOCK. */
	uint64_t data_fragment_bytes_dropped_would_block;
	/*! \brief Data fragments dropped due to portal not being operational. */
	uint64_t data_fragments_dropped_not_operational;
	/*! \brief Data fragment bytes dropped due to portal not being operational. */
	uint64_t data_fragment_bytes_dropped_not_operational;
	/*! \brief Data fragments dropped due to queueing failure. */
	uint64_t data_fragments_dropped_queue_failure;
	/*! \brief Data fragment bytes dropped due to queueing failure. */
	uint64_t data_fragment_bytes_dropped_queue_failure;

	/*! \brief Unicast messages forwarded to this portal. */
	uint64_t unicast_msgs_forwarded;
	/*! \brief Unicast message bytes forwarded to this portal. */
	uint64_t unicast_msg_bytes_forwarded;
	/*! \brief Unicast messages sent. */
	uint64_t unicast_msgs_sent;
	/*! \brief Unicast message bytes sent. */
	uint64_t unicast_msg_bytes_sent;
	/*! \brief Unicast messages dropped due to EWOULDBLOCK. */
	uint64_t unicast_msgs_dropped_would_block;
	/*! \brief Unicast message bytes dropped due to EWOULDBLOCK. */
	uint64_t unicast_msg_bytes_dropped_would_block;
	/*! \brief Unicast messages dropped due to portal not being operational. */
	uint64_t unicast_msgs_dropped_not_operational;
	/*! \brief Unicast message bytes dropped due to portal not being operational. */
	uint64_t unicast_msg_bytes_dropped_not_operational;
	/*! \brief Unicast messages dropped due to queueing failure. */
	uint64_t unicast_msgs_dropped_queue_failure;
	/*! \brief Unicast message bytes dropped due to queueing failure. */
	uint64_t unicast_msg_bytes_dropped_queue_failure;

	/*! \brief DRO control messages. */
	uint64_t gateway_cntl_msgs;
	/*! \brief DRO control message bytes. */
	uint64_t gateway_cntl_msg_bytes;
	/*! \brief DRO control messages sent. */
	uint64_t gateway_cntl_msgs_sent;
	/*! \brief DRO control message bytes sent. */
	uint64_t gateway_cntl_msg_bytes_sent;
	/*! \brief DRO control messages dropped due to EWOULDBLOCK. */
	uint64_t gateway_cntl_msgs_dropped_would_block;
	/*! \brief DRO control message bytes dropped due to EWOULDBLOCK. */
	uint64_t gateway_cntl_msg_bytes_dropped_would_block;
	/*! \brief DRO control messages dropped due to portal not being operational. */
	uint64_t gateway_cntl_msgs_dropped_not_operational;
	/*! \brief DRO control message bytes dropped due to portal not being operational. */
	uint64_t gateway_cntl_msg_bytes_dropped_not_operational;
	/*! \brief DRO control messages dropped due to queueing failure. */
	uint64_t gateway_cntl_msgs_dropped_queue_failure;
	/*! \brief DRO control message bytes dropped due to queueing failure. */
	uint64_t gateway_cntl_msg_bytes_dropped_queue_failure;

	/*! \brief Number of message batches. */
	uint64_t batches;
	/*! \brief Minimum number of messages per batch. */
	uint64_t batch_msgs_min;
	/*! \brief Mean number of messages per batch. */
	uint64_t batch_msgs_mean;
	/*! \brief Maximum number of messages per batch. */
	uint64_t batch_msgs_max;
	/*! \brief Minimum number of bytes per batch. */
	uint64_t batch_bytes_min;
	/*! \brief Mean number of bytes per batch. */
	uint64_t batch_bytes_mean;
	/*! \brief Maximum number of bytes per batch. */
	uint64_t batch_bytes_max;
	/*! \brief Current data bytes enqueued internally. */
	uint64_t data_bytes_enqueued;
	/*! \brief Maximum data bytes enqueued internally. */
	uint64_t data_bytes_enqueued_max;
	/*! \brief Configured maximum data bytes allowed in queued. */
	uint64_t data_bytes_enqueued_limit;

	/*! \brief Total RTT samples. */
	uint64_t rtt_samples;
	/*! \brief Minimum RTT to companion. */
	uint64_t rtt_min;
	/*! \brief Mean RTT to companion. */
	uint64_t rtt_mean;
	/*! \brief Maximum RTT to companion. */
	uint64_t rtt_max;
	/*! \brief Last keepalive responded to. */
	time_t last_ka_time;
} tnwg_dstat_peer_send_stats_t;


/*! \brief Structure containing sending and receiving statistics for an endpoint portal.
	Used by \ref tnwg_portalstats_dstat_record_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_portal_endpoint_dstat_record_t_stct {
	/*! \brief Receiving statistics. */
	tnwg_dstat_endpoint_receive_stats_t receive_stats;
	
	/*! \brief Sending statistics. */
	tnwg_dstat_endpoint_send_stats_t send_stats;

	/*! \brief Topic Resolution Domain (TRD) for the portal. */
	lbm_uint32_t domain_id;
	/*! \brief Cost. */
	lbm_uint32_t ingress_cost;
	/*! \brief Local interest topics. */
	lbm_uint32_t local_interest_topics;
	/*! \brief Local interest PCRE patterns. */
	lbm_uint32_t local_interest_pcre_patterns;
	/*! \brief Local interest regex patterns. */
	lbm_uint32_t local_interest_regex_patterns;
	/*! \brief Remote interest topics. */
	lbm_uint32_t remote_interest_topics;
	/*! \brief Remote interest PCRE patterns. */
	lbm_uint32_t remote_interest_pcre_patterns;
	/*! \brief Remote interest regex patterns. */
	lbm_uint32_t remote_interest_regex_patterns;
	/*! \brief Proxy receivers. */
	lbm_uint32_t proxy_receivers;
	/*! \brief Receiver topics. */
	lbm_uint32_t receiver_topics;
	/*! \brief Receiver PCRE patterns. */
	lbm_uint32_t receiver_pcre_patterns;
	/*! \brief Receiver regex patterns. */
	lbm_uint32_t receiver_regex_patterns;
	/*! \brief Proxy sources. */
	lbm_uint32_t proxy_sources;	
} tnwg_portal_endpoint_dstat_record_t;

/*! \brief Structure containing sending and receiving statistics for a peer portal.
	Used by \ref tnwg_portalstats_dstat_record_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_portal_peer_dstat_record_t_stct {
	/*! \brief Receiving statistics. */
	tnwg_dstat_peer_receive_stats_t receive_stats;
	
	/*! \brief Sending statistics. */
	tnwg_dstat_peer_send_stats_t send_stats;

	/*! \brief Cost. */
	uint32_t ingress_cost;
	/*! \brief Number of local interest topics. */
	uint32_t local_interest_topics;
	/*! \brief Number of local interest pcre patterns. */
	uint32_t local_interest_pcre_patterns;
	/*! \brief Number of local interest regex patterns. */
	uint32_t local_interest_regex_patterns;
	/*! \brief Number of remote interest topics. */
	uint32_t remote_interest_topics;
	/*! \brief Number of remote interest pcre patterns. */
	uint32_t remote_interest_pcre_patterns;
	/*! \brief Number of remote interest regex patterns. */
	uint32_t remote_interest_regex_patterns;
	/*! \brief Number of proxy receivers. */
	uint32_t proxy_receivers;
	/*! \brief Number of receiver topics. */
	uint32_t receiver_topics;
	/*! \brief Number of receiver pcre patterns. */
	uint32_t receiver_pcre_patterns;
	/*! \brief Number of receiver regex patterns. */
	uint32_t receiver_regex_patterns;
	/*! \brief Number of proxy sources. */
	uint32_t proxy_sources;	
} tnwg_portal_peer_dstat_record_t;

/*! \brief Structure containing sending and receiving statistics for peer or endpoint portal.
	Used by \ref tnwg_dstat_portalstats_msg_t.
	The full length of this structure is defined by the longest of its union
	elements.
	However, when messages are sent, the actual size of the selected union
	element is used.
	That size is recorded in \ref tnwg_dstat_record_hdr_t_stct::record_length.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_portalstats_dstat_record_t_stct {
	/*! \brief The following union is selected by
		\ref tnwg_dstat_portalstats_msg_t_stct::rechdr.`portal_type`. */
	union {
		/*! \brief Union element selected by tnwg_dstat_record_hdr_t_stct::portal_type ==
			\ref TNWG_DSTAT_Portal_Type_Peer. */
		tnwg_portal_endpoint_dstat_record_t endpt;
		/*! \brief Union element selected by tnwg_dstat_record_hdr_t_stct::portal_type ==
			\ref TNWG_DSTAT_Portal_Type_Endpoint. */
		tnwg_portal_peer_dstat_record_t peer;
	} ptype;
} tnwg_portalstats_dstat_record_t;

/*! \brief Structure containing general information about a peer or endpoint portal.
	Used by \ref tnwg_dstat_portalstats_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_record_hdr_t_stct {
	/*! \brief Portal type:
		\ref TNWG_DSTAT_Portal_Type_Peer
		\ref TNWG_DSTAT_Portal_Type_Endpoint. */
	uint32_t portal_type;
	/*! \brief Portal index. */
	uint32_t index;
	/*! \brief The length of the \ref tnwg_portalstats_dstat_record_t structure
		after the union selection is made. */
	uint32_t record_length;
	/*! \brief Portal name. */
	char portal_name[TNWG_DSTAT_MAX_PORTAL_NAME_LEN];
} tnwg_dstat_record_hdr_t;

/*! \brief Message containing portal statistics.
	<br>( \ref tnwg_dstat_msg_hdr_t_stct::type == \ref TNWG_DSTATTYPE_PORTSTAT )<br>
	This message can have different lengths, depending on which element of
	the union tnwg_portalstats_dstat_record_t_stct::ptype is selected,
	based on the value of tnwg_dstat_record_hdr_t_stct::portal_type.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_portalstats_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	tnwg_dstat_msg_hdr_t msghdr;
	/*! \brief General information about this portal. */
	tnwg_dstat_record_hdr_t rechdr;
	/*! \brief Type-specific information about this portal. */
	tnwg_portalstats_dstat_record_t record;
} tnwg_dstat_portalstats_msg_t;


/*! \brief Message containing DRO configuration information.
	<br>( \ref tnwg_dstat_msg_hdr_t_stct::type == \ref TNWG_DSTATTYPE_GATEWAYCFG )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_dstat_gatewaycfg_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	tnwg_dstat_msg_hdr_t msghdr;
	/*! \brief String data containing DRO configuration.
		See \ref umrouterdaemonstatisticsstructuresstringbuffers. */
	char data;
} tnwg_dstat_gatewaycfg_msg_t;


#define MAX_ATTR_TYPE_NAME_LEN 64
/*! \brief Structure containing portal configuration information.
	Used by \ref tnwg_pcfg_stat_grp_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_pcfg_stat_grp_rec_hdr_t_stct {
	/*! \brief Portal number */
	uint32_t portal_num;
	/*! \brief Portal type:
		\ref TNWG_DSTAT_Portal_Type_Peer
		\ref TNWG_DSTAT_Portal_Type_Endpoint. */
	uint32_t portal_type;
	/*! \brief Number of options stored in \ref tnwg_pcfg_stat_grp_msg_t_stct::data. */
	uint32_t num_options;
	/*! \brief Name of portal. */
	char portal_name[TNWG_DSTAT_MAX_PORTAL_NAME_LEN];
	/*! \brief Type of options stored in \ref tnwg_pcfg_stat_grp_msg_t_stct::data. */
	char attr_type[MAX_ATTR_TYPE_NAME_LEN];
} tnwg_pcfg_stat_grp_rec_hdr_t;

/*! \brief Message containing memory allocation statistics.
	<br>( \ref tnwg_dstat_msg_hdr_t_stct::type == \ref TNWG_DSTATTYPE_PORTCFG )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_pcfg_stat_grp_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	tnwg_dstat_msg_hdr_t msghdr;
	/*! \brief Structure containing portal configuration information. */
	tnwg_pcfg_stat_grp_rec_hdr_t rechdr;
	/*! \brief String data containing portal configuration attributes.
		The number of strings is given by tnwg_pcfg_stat_grp_msg_t_stct::rechdr->num_options.
		See \ref umrouterdaemonstatisticsstructuresstringbuffers for
		more information. */
	char data;
} tnwg_pcfg_stat_grp_msg_t;

/******************************Route Manager Topology****************************/

/*! \brief Structure containing high-level information about this UM Router's (DRO)
	view of the current topology.
	Used by \ref tnwg_rm_stat_grp_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_rm_stat_grp_local_info_t_stct {
	/*! \brief Name of this DRO. */
	char gateway_name[TNWG_DSTAT_MAX_GATEWAY_NAME_LEN];
	/*! \brief DRO id. */
	uint64_t gateway_id;
	/*! \brief Topology signature. */
	uint32_t topology_signature;
	/*! \brief Recalc duration sec. */
	uint32_t recalc_duration_sec;
	/*! \brief Recalc duration usec. */
	uint32_t recalc_duration_usec;
	/*! \brief Graph version. */
	uint32_t graph_version;
	/*! \brief Self version. */
	uint16_t self_version;
	/*! \brief DRO count. */
	uint16_t gateway_count;
	/*! \brief Trd count. */
	uint16_t trd_count;
} tnwg_rm_stat_grp_local_info_t;

/*! \brief Structure containing routing information specific to a particular
	portal.
	Used by \ref tnwg_rm_stat_grp_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_rm_stat_grp_portal_info_t_stct {
	/*! \brief ID of adjacent DRO (for `type` == \ref TNWG_DSTAT_Portal_Type_Peer),
		or Topic Resolution Domain ID (for `type` == \ref TNWG_DSTAT_Portal_Type_Endpoint). */
	uint64_t node_or_adjacent_id;
	/*! \brief Portal type:
		\ref TNWG_DSTAT_Portal_Type_Peer
		\ref TNWG_DSTAT_Portal_Type_Endpoint. */
	uint32_t type;
	/*! \brief Portal index. */
	uint32_t index;
	/*! \brief Cost. */
	uint32_t ingress_cost;
	/*! \brief Recalc duration sec. */
	uint32_t recalc_duration_sec;
	/*! \brief Recalc duration usec. */
	uint32_t recalc_duration_usec;
	/*! \brief Proxy rec recalc duration sec. */
	uint32_t proxy_rec_recalc_duration_sec;
	/*! \brief Proxy rec recalc duration usec. */
	uint32_t proxy_rec_recalc_duration_usec;
	/*! \brief Portal name. */
	char portal_name[TNWG_DSTAT_MAX_PORTAL_NAME_LEN];
} tnwg_rm_stat_grp_portal_info_t;


/*! \brief Each UM Router (DRO) tracks a small amount of information about every
	other DRO in the network.  This structure represents this DRO's view of a
	remote DRO's \ref tnwg_rm_stat_grp_local_info_t.
	Used by \ref tnwg_rm_stat_grp_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_rm_stat_grp_othergw_info_t_stct {
	/*! \brief DRO identifier. */
	uint64_t gateway_id;
	/*! \brief Topology signature. */
	uint32_t topology;
	/*! \brief Approximate timestamp of last activity.
		Represents local wall clock time from the sending host's perspective.
		Value is "POSIX Time" (seconds since 1-Jan-1970),
		but in sending host's timezone. */
	uint32_t last_activity_sec;
	/*! \brief Count of microseconds to be added to "last_activity_sec" to increase
		the precision of the timestamp.  However, the accuracy of the
		timestamp is not guaranteed to be at the microsecond level. */
	uint32_t last_activity_usec;
	/*! \brief This version number essentially represents the number of times the DRO
		has experienced a change in the number times a peer link has changed state.
		When the version number changes, other DROs assume that the topology has changed
		and re-run the routing logic. */
	uint16_t version;
	/*! \brief Number of running portals.  This number can change as peer links connect
		and disconnect. */
	uint16_t num_neighbors;
	/*! \brief Name of DRO. */
	char gateway_name[TNWG_DSTAT_MAX_GATEWAY_NAME_LEN];
} tnwg_rm_stat_grp_othergw_info_t;


/*! \brief Structure containing this UM Router's (DRO) view of a remote DRO's
	portal information.
	Used by \ref tnwg_rm_stat_grp_msg_t.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_rm_stat_grp_othergw_neighbor_t_stct {
	/*! \brief Node ID. */
	uint64_t node_id;
	/*! \brief 0 for domain, 1 for DRO. */
	uint32_t domain_or_gateway;
	/*! \brief Cost from this node_id to the owning neighbor_info's node_id. */
	uint32_t ingress_cost;
} tnwg_rm_stat_grp_othergw_neighbor_t;


/*! \brief Message containing routing information.
	<br>( \ref tnwg_dstat_msg_hdr_t_stct::type == \ref TNWG_DSTATTYPE_RM_LOCAL,
	<br>&nbsp;&nbsp;\ref TNWG_DSTATTYPE_RM_PORTAL, \ref TNWG_DSTATTYPE_RM_OTHERGW,
	<br>&nbsp;&nbsp;\ref TNWG_DSTATTYPE_RM_OTHERGW_NBR )<br>
	This message can have different lengths, depending on which element of
	the union `msgtype` is selected,
	based on the message type in tnwg_dstat_msg_hdr_t_stct::type.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_TNWG_DAEMON_ANTIMAGIC. */
typedef struct tnwg_rm_stat_grp_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	tnwg_dstat_msg_hdr_t msghdr;
	/*! \brief union containing the different types of route information messages. */
	union {
		/*! \brief Use when \ref tnwg_dstat_msg_hdr_t_stct::type == \ref TNWG_DSTATTYPE_RM_LOCAL. */
		tnwg_rm_stat_grp_local_info_t local;
		/*! \brief Use when \ref tnwg_dstat_msg_hdr_t_stct::type == TNWG_DSTATTYPE_RM_PORTAL. */
		tnwg_rm_stat_grp_portal_info_t portal;
		/*! \brief Use when \ref tnwg_dstat_msg_hdr_t_stct::type == TNWG_DSTATTYPE_RM_OTHERGW. */
		tnwg_rm_stat_grp_othergw_info_t othergw;
		/*! \brief Use when \ref tnwg_dstat_msg_hdr_t_stct::type == TNWG_DSTATTYPE_RM_OTHERGW_NBR. */
		tnwg_rm_stat_grp_othergw_neighbor_t neighbor;
	} msgtype;
} tnwg_rm_stat_grp_msg_t;

#endif /* TNWG_DMON_MSGS_H */
