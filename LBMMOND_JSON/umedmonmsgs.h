/**     \file umedmonmsgs.h
        \brief Ultra Messaging (UM) message definitions for Persistent Store
	Daemon Statistics.
	For general information on Daemon Statistics, see \ref storedaemonstatistics.

  All of the documentation and software included in this and any
  other Informatica Corporation Ultra Messaging Releases
  Copyright (C) Informatica Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted only as covered by the terms of a
  valid software license agreement with Informatica Corporation.

  Copyright (C) 2007-2017, Informatica Corporation. All Rights Reserved.

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

#ifndef UME_DMON_MSGS_H
#define UME_DMON_MSGS_H

#define LBM_UMESTORE_DMON_VERSION_STRLEN 256
#define LBM_UMESTORE_DMON_STORE_NAME_STRLEN 256
#define LBM_UMESTORE_DMON_TOPIC_PATTERN_STRLEN 256
#define LBM_UMESTORE_DMON_FILENAME_MAX_STRLEN 256

/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	memory statistics message of type:
	\ref umestore_smart_heap_dmon_stat_msg_t */
#define LBM_UMESTORE_DMON_MPG_SMART_HEAP_STATS 1
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	Store statistics message of type:
	\ref umestore_store_dmon_stat_msg_t */
#define LBM_UMESTORE_DMON_MPG_STORE_STATS 2
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	Repository statistics message (associated with a source) of type:
	\ref umestore_repo_dmon_stat_msg_t */
#define LBM_UMESTORE_DMON_MPG_REPO_STATS 3
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	disk statistics message of type:
	\ref umestore_disk_dmon_stat_msg_t */
#define LBM_UMESTORE_DMON_MPG_DISK_STATS 4
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	receiver statistics message of type:
	\ref umestore_rcv_dmon_stat_msg_t */
#define LBM_UMESTORE_DMON_MPG_RCV_STATS 5
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	Store configuration message of type:
	\ref umestore_store_dmon_config_msg_t */
#define LBM_UMESTORE_DMON_MPG_STORE_CONFIG 101
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	Store topic pattern message of type:
	\ref umestore_store_pattern_dmon_config_msg_t */
#define LBM_UMESTORE_DMON_MPG_STORE_PATTERN_CONFIG 102
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	Store topic match message of type:
	\ref umestore_topic_dmon_config_msg_t */
#define LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG 103
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	repository configuration message (associated with a source) of type:
	\ref umestore_repo_dmon_config_msg_t */
#define LBM_UMESTORE_DMON_MPG_REPO_CONFIG 104
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::type for a
	receiver configuration message of type:
	\ref umestore_rcv_dmon_config_msg_t */
#define LBM_UMESTORE_DMON_MPG_RCV_CONFIG 105

/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::magic indicating that
	the message does NOT need byte swapping. */
#define LBM_UMESTORE_DMON_MAGIC 0x4542
/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::magic indicating that
	the message DOES need byte swapping. */
#define LBM_UMESTORE_DMON_ANTIMAGIC 0x4245

/*! \brief Value of \ref umestore_dmon_msg_hdr_t_stct::version indicating
	the version of the monitoring daemon.  See \ref daemonstatisticsversioning
	for general information on versioning of these structures. */
#define LBM_UMESTORE_DMON_VERSION 0

/*! \brief Common message header structure included at the start of all
	messages. */
typedef struct umestore_dmon_msg_hdr_t_stct {
	/*! \brief "Magic" value set by sender to indicate to the receiver
		whether byte swapping is needed.
		Possible values: \ref LBM_UMESTORE_DMON_MAGIC, \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
	lbm_uint16_t magic;
	/*! \brief Message type set by sender to indicate which kind of message
		this is.
		Possible values: one of the LBM_UMESTORE_DMON_MPG_* constants
		( \ref LBM_UMESTORE_DMON_MPG_STORE_STATS,
		\ref LBM_UMESTORE_DMON_MPG_REPO_STATS, etc. ) */
	lbm_uint16_t type;
	/*! \brief Version of the message definition.  See
		\ref daemonstatisticsversioning for general information on versioning
		of these structures. */
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
		the precision of the timestamp.
		However, the accuracy of the timestamp is not guaranteed to be at the
		microsecond level. */
	lbm_uint32_t tv_usec;
} umestore_dmon_msg_hdr_t;
#define UMESTORE_DMON_MSG_HDR_T_SZ (offsetof(umestore_dmon_msg_hdr_t, tv_usec) + sizeof(lbm_uint32_t))

/* ************************************************************************************************** */
/* configuration messages                                                                             */
/* ************************************************************************************************** */

/*! \brief Message containing unchanging daemon information set at store
	daemon startup.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_STORE_CONFIG. )<br>
	One or more of these will be published for a given daemon, one for each
	\ref storeelement configured.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_store_dmon_config_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief The network address of the store, as configured with the
		`interface` attribute of the \ref storeelement.
		Format: as returned by standard function `inet_addr()` - a 32-bit
		unsigned integer in network byte order.  Can be converted back to
		ASCII dotted notation using inet_ntoa().
		<b>NOTE</b>: This field should NOT be byte-swapped. */
	lbm_uint32_t store_iface;
	/*! \brief Value configured for the store's option
		\ref umecfgretransmissionrequestprocessingrate "retransmission-request-processing-rate". */
	lbm_uint32_t store_max_retransmission_processing_rate;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief Value configured for the \ref storeelement's `port` attribute. */
	lbm_uint16_t store_port;
	/*! \brief Byte offset from start of structure to start of store name
		string (null-terminated). */
	lbm_uint16_t store_name_offset;
	/*! \brief Byte offset from start of structure to start of store's cache
		directory name string (null-terminated). */
	lbm_uint16_t disk_cache_dir_offset;
	/*! \brief Byte offset from start of structure to start of store's state
		directory name string (null-terminated). */
	lbm_uint16_t disk_state_dir_offset;
	/*! \brief String buffer to hold string data associated with the
		`*_offset` fields (e.g. store_name_offset).
		See those `*_offset` fields for descriptions and starting positions
		of the strings.
		Stored as a null-terminated strings of variable length.
		I.e. the buffer in this structure is declared as the maximum possible
		size.
		But only as many bytes as necessary are actually sent. */
	char string_buffer[LBM_UMESTORE_DMON_STORE_NAME_STRLEN + (2 * LBM_UMESTORE_DMON_FILENAME_MAX_STRLEN)];
} umestore_store_dmon_config_msg_t;
#define UMESTORE_STORE_DMON_CONFIG_MSG_T_MIN_SZ (offsetof(umestore_store_dmon_config_msg_t, string_buffer) + 2)

/*! \brief Message containing unchanging store pattern information set at
	store daemon startup.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_STORE_PATTERN_CONFIG )<br>
	One or more of these will be published for a given store, one for each
	\ref topicelement configured.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_store_pattern_dmon_config_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief Value configured for the \ref topicelement's attribute
		\ref umecfgtype "type" attribute.
		Possible values: \ref UMESTORE_DMON_TOPIC_TYPE_DIRECT,
		\ref UMESTORE_DMON_TOPIC_TYPE_PCRE,
		\ref UMESTORE_DMON_TOPIC_TYPE_REGEXP. */
	lbm_uint16_t type;
	/*! \brief Topic name or pattern configured for the \ref topicelement's
		attribute \ref umecfgpattern "pattern".
		Stored as a null-terminated string of variable length.
		I.e. the buffer in this structure is declared as the maximum possible
		size.
		But only as many bytes as necessary are actually sent. */
	char pattern_buffer[LBM_UMESTORE_DMON_TOPIC_PATTERN_STRLEN];
} umestore_store_pattern_dmon_config_msg_t;
#define UMESTORE_STORE_PATTERN_DMON_CONFIG_MSG_T_MIN_SZ (offsetof(umestore_store_pattern_dmon_config_msg_t, pattern_buffer) + 2)

/*! \brief Value for umestore_store_pattern_dmon_config_msg_t_stct::type
	indicating that the pattern must be an exact match for the desired topic. */
#define UMESTORE_DMON_TOPIC_TYPE_DIRECT 0
/*! \brief Value for umestore_store_pattern_dmon_config_msg_t_stct::type
	indicating that the pattern is a PCRE-style regular expression to
	match the desired topics. */
#define UMESTORE_DMON_TOPIC_TYPE_PCRE 1
/*! \brief Value for umestore_store_pattern_dmon_config_msg_t_stct::type
	indicating that the pattern is a POSIX-style regular expression to
	match the desired topics.  This type is deprecated. */
#define UMESTORE_DMON_TOPIC_TYPE_REGEXP 2

/*! \brief Message containing topic name of one or more registered sources.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG )<br>
	Zero or more of these will be published for a given store, one for each
	topic that has at least one source registration.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_topic_dmon_config_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Identifying index number for this topic, used to establish
		relationships with other data structures. */
	lbm_uint32_t dmon_topic_idx;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief Topic name of registered source.
		Matches the configured attribute \ref umecfgpattern "pattern".
		Stored as a null-terminated string of variable length.
		I.e. the buffer in this structure is declared as the maximum possible
		size.
		But only as many bytes as necessary are actually sent. */
	char topic_name[LBM_UMESTORE_DMON_TOPIC_PATTERN_STRLEN];
} umestore_topic_dmon_config_msg_t;
#define UMESTORE_TOPIC_DMON_CONFIG_MSG_T_MIN_SZ (offsetof(umestore_topic_dmon_config_msg_t, topic_name) + 2)

/*! \brief Message containing repository configuration defined during
	source registration.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_REPO_CONFIG )<br>
	Zero or more of these will be published for a given store, one for each
	source which registers with the store.
	Many of the fields are not necessarily set to the value configured on
	the store, but instead can be overridden by the source.
	See each option's documentation for details.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_repo_dmon_config_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Session ID used by the source to register.
		(Session IDs are optional; if the source did not specify a
		Session ID, this will be zero.) */
	lbm_uint64_t sid;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorydiskfilesizelimit "repository-disk-file-size-limit". */
	lbm_uint64_t disk_sz_limit;
	/*! \brief Value configured for the store's option
		\ref umecfgsourceflightsizebytesmaximum "source-flight-size-bytes-maximum". */
	lbm_uint64_t src_flightsz_bytes;
	/*! \brief Identifying index number for the topic of this source,
		defined in an earlier message of type
		\ref LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG.
		See \ref umestore_topic_dmon_config_msg_t_stct::dmon_topic_idx. */
	lbm_uint32_t dmon_topic_idx;
	/*! \brief Registration ID used by the source to register. */
	lbm_uint32_t regid;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorysizethreshold "repository-size-threshold". */
	lbm_uint32_t sz_threshold;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorysizelimit "repository-size-limit". */
	lbm_uint32_t sz_limit;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositoryagethreshold "repository-age-threshold". */
	lbm_uint32_t age_threshold;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorydiskmaxwriteasynccbs "repository-disk-max-write-async-cbs". */
	lbm_uint32_t disk_max_write_aiocbs;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorydiskmaxreadasynccbs "repository-disk-max-read-async-cbs". */
	lbm_uint32_t disk_max_read_aiocbs;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorydiskasyncbufferlength "repository-disk-async-buffer-length". */
	lbm_uint32_t disk_aio_buffer_len;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorydiskwritedelay "repository-disk-write-delay". */
	lbm_uint32_t write_delay;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositorytype "repository-type".
		Possible values: \ref UMESTORE_DMON_REPO_TYPE_NOCACHE
		\ref UMESTORE_DMON_REPO_TYPE_MEMORY
		\ref UMESTORE_DMON_REPO_TYPE_DISK
		\ref UMESTORE_DMON_REPO_TYPE_REDUCED_FD. */
	lbm_uint8_t type;
	/*! \brief Value configured for the store's option
		\ref umecfgrepositoryallowackonreception "repository-allow-ack-on-reception".
		Possible values: \ref UMESTORE_DMON_REPO_DO_NOT_ACK_ON_RECEPTION,
		\ref UMESTORE_DMON_REPO_ACK_ON_RECEPTION. */
	lbm_uint8_t allow_ack_on_reception;
} umestore_repo_dmon_config_msg_t;
#define UMESTORE_REPO_DMON_CONFIG_MSG_T_SZ (offsetof(umestore_repo_dmon_config_msg_t, allow_ack_on_reception) + sizeof(lbm_uint8_t))

/*! \brief Value for umestore_repo_dmon_config_msg_t_stct::type
	indicating a "no-cache" repository. */
#define UMESTORE_DMON_REPO_TYPE_NOCACHE 0
/*! \brief Value for umestore_repo_dmon_config_msg_t_stct::type
	indicating a "memory" repository. */
#define UMESTORE_DMON_REPO_TYPE_MEMORY 1
/*! \brief Value for umestore_repo_dmon_config_msg_t_stct::type
	indicating a "disk" repository. */
#define UMESTORE_DMON_REPO_TYPE_DISK 2
/*! \brief Value for umestore_repo_dmon_config_msg_t_stct::type
	indicating a "reduced-fd" repository. */
#define UMESTORE_DMON_REPO_TYPE_REDUCED_FD 3

/*! \brief Value for umestore_repo_dmon_config_msg_t_stct::allow_ack_on_reception
	indicating that the store allows "ack on reception" behavior. */
#define UMESTORE_DMON_REPO_DO_NOT_ACK_ON_RECEPTION 0
/*! \brief Value for umestore_repo_dmon_config_msg_t_stct::allow_ack_on_reception
	indicating that the store does not allow "ack on reception" behavior. */
#define UMESTORE_DMON_REPO_ACK_ON_RECEPTION 1

/*! \brief Message containing repository configuration defined during
	receiver registration.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_RCV_CONFIG )<br>
	Zero or more of these will be published for a given store, one for each
	receiver which registers with the store.
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_rcv_dmon_config_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Session ID used by the receiver to register.
		(Session IDs are optional; if the receiver did not specify a
		Session ID, this will be zero.) */
	lbm_uint64_t sid;
	/*! \brief Identifying index number for the topic of this source,
		defined in an earlier message of type
		\ref LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG.
		See \ref umestore_topic_dmon_config_msg_t_stct::dmon_topic_idx. */
	lbm_uint32_t dmon_topic_idx;
	/*! \brief Registration ID used by the receiver to register. */
	lbm_uint32_t regid;
	/*! \brief The receiver's view of the transport session index. */
	lbm_uint32_t transport_idx;
	/*! \brief The receiver's view of the topic index within the transport session. */
	lbm_uint32_t topic_idx;
	/*! \brief Topic Resolution Domain ID of the receiver. */
	lbm_uint32_t domain_id;
	/*! \brief The network address of the receiver.
		Format: as returned by standard function `inet_addr()` - a 32-bit
		unsigned integer in network byte order.  Can be converted back to
		ASCII dotted notation using inet_ntoa().
		<b>NOTE</b>: This field should NOT be byte-swapped. */
	lbm_uint32_t sin_addr;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief "Request port" configured for the receiver. */
	lbm_uint16_t sin_port;
} umestore_rcv_dmon_config_msg_t;
#define UMESTORE_RCV_DMON_CONFIG_MSG_T_SZ (offsetof(umestore_rcv_dmon_config_msg_t, sin_port) + sizeof(lbm_uint16_t))

/* ************************************************************************************************** */
/* statistics messages                                                                                */
/* ************************************************************************************************** */

/*! \brief Message containing smart heap statistics.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_SMART_HEAP_STATS )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_smart_heap_dmon_stat_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Memory usage (bytes) as reported by SmartHeap's MemPoolSize()
		function. */
	lbm_int64_t poolsize;
	/*! \brief Active allocation count (bytes) as reported by SmartHeap's
		MemPoolCount() function. */
	lbm_int64_t poolcount;
	/*! \brief Small block size (bytes) as reported by SmartHeap's
		MemPoolInfo() function. */
	lbm_uint32_t smallBlockSize;
	/*! \brief Page size (bytes) as reported by SmartHeap's
		MemPoolInfo() function. */
	lbm_uint32_t pageSize;
	/*! \brief SmartHeap major version number. */
	lbm_uint16_t mem_major_version;
	/*! \brief SmartHeap minor version number. */
	lbm_uint16_t mem_minor_version;
	/*! \brief SmartHeap update version number. */
	lbm_uint16_t mem_update_version;
	/*! \brief UM Store version.
		Stored as a null-terminated string of variable length. */
	char umestored_version_buffer[LBM_UMESTORE_DMON_VERSION_STRLEN];
} umestore_smart_heap_dmon_stat_msg_t;
#define UMESTORE_SMART_HEAP_DMON_STAT_MSG_T_MIN_SZ (offsetof(umestore_smart_heap_dmon_stat_msg_t, umestored_version_buffer) + 2)

/*! \brief Message containing store message statistics.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_STORE_STATS )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_store_dmon_stat_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Retransmission Request Received count. */
	lbm_uint32_t ume_retx_req_rcv_count;
	/*! \brief Retransmission Request Service count. */
	lbm_uint32_t ume_retx_req_serviced_count;
	/*! \brief Retransmission Request Drop count. */
	lbm_uint32_t ume_retx_req_drop_count;
	/*! \brief Seconds since the user last cleared the retransmit stats.
		If the user has not cleared them, it represents seconds since the
		store was started. */
	lbm_uint32_t ume_retx_stat_interval;
	/*! \brief Retransmission Request Total Dropped. */
	lbm_uint32_t ume_retx_req_total_dropped;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
} umestore_store_dmon_stat_msg_t;
#define UMESTORE_STORE_DMON_STAT_MSG_T_SZ (offsetof(umestore_store_dmon_stat_msg_t, store_idx) + sizeof(lbm_uint16_t))

/*! \brief Message containing store repository statistics.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_REPO_STATS )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_repo_dmon_stat_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Identifying index number for the topic of this source,
		defined in an earlier message of type
		\ref LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG.
		See \ref umestore_topic_dmon_config_msg_t_stct::dmon_topic_idx. */
	lbm_uint32_t dmon_topic_idx;
	/*! \brief Registration ID used by the source to register. */
	lbm_uint32_t regid;
	/*! \brief Total number of UM message fragments the store has for
		this source, both on disk and in memory. */
	lbm_uint32_t message_map_sz;
	/*! \brief The number of bytes of messages in memory for a non-RPP
		repository (`flags & UMESTORE_DMON_REPO_FLAG_RPP == 0`).
		Includes headers and store overhead. */
	lbm_uint32_t memory_sz;
	/*! \brief The number of bytes of messages in memory for a RPP
		repository (`flags & UMESTORE_DMON_REPO_FLAG_RPP != 0`).
		Includes headers and store overhead. */
	lbm_uint32_t rpp_memory_sz;
	/*! \brief Lead sequence number: newest sequence number in the store. */
	lbm_uint32_t lead_sqn;
	/*! \brief Most recent sequence number for which the store has initiated
		persisting to disk, but the Operating System has not confirmed
		completion of persistence. */
	lbm_uint32_t sync_sqn;
	/*! \brief Most recent sequence number that the Operating System has
		confirmed persisting to disk. */
	lbm_uint32_t sync_complete_sqn;
	/*! \brief Trail sequence number: oldest sequence number in the store. */
	lbm_uint32_t trail_sqn;
	/*! \brief Trail sequence number: oldest sequence number in memory. */
	lbm_uint32_t mem_trail_sqn;
	/*! \brief Most recent sequence number that along with the trail_sqn,
		creates a range of sequence numbers with no sequence number gaps. */
	lbm_uint32_t contig_sqn;
	/*! \brief The highest sequence number reported among any Unrecoverable
		Loss Burst (ULB) event.
		It is not reset after the ULB is handled; it is maintained
		throughout the life of the store. */
	lbm_uint32_t high_ulb_sqn;
	/*! \brief Number of messages in the process of being dropped due to
		reaching the \ref umecfgrepositorysizelimit "repository-size-limit". */
	lbm_uint32_t map_intentional_drops;
	/*! \brief Number of messages lost unrecoverably, not including
		burst loss events. */
	lbm_uint32_t uls;
	/*! \brief Number of messages lost unrecoverably due to burst loss
		events. */
	lbm_uint32_t ulbs;
	/*! \brief Total number of messages dropped due to
		reaching the \ref umecfgrepositorysizelimit "repository-size-limit". */
	lbm_uint32_t sz_limit_drops;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief Bit map of flags indicating various characteristics and
		current states of the store.  Possible bits:
		\ref UMESTORE_DMON_REPO_FLAG_INTENTIONAL_DROPS,
		\ref UMESTORE_DMON_REPO_FLAG_ISN_SET,
		\ref UMESTORE_DMON_REPO_FLAG_DISK_CKSUM,
		\ref UMESTORE_DMON_REPO_FLAG_SHUTTING_DOWN,
		\ref UMESTORE_DMON_REPO_FLAG_RPP,
		\ref UMESTORE_DMON_REPO_FLAG_DELAY_WRITE. */
	lbm_uint8_t flags;
} umestore_repo_dmon_stat_msg_t;
#define UMESTORE_REPO_DMON_STAT_MSG_T_SZ (offsetof(umestore_repo_dmon_stat_msg_t, flags) + sizeof(lbm_uint8_t))

/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the store is currently dropping messages due to reaching
	the \ref umecfgrepositorysizelimit "repository-size-limit". */
#define UMESTORE_DMON_REPO_FLAG_INTENTIONAL_DROPS 0x1
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the repository has an initial sequence number for this
	source. */
#define UMESTORE_DMON_REPO_FLAG_ISN_SET 0x2
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating if checksumming is enabled.
	See \ref umecfgrepositorydiskmessagechecksum "repository-disk-message-checksum". */
#define UMESTORE_DMON_REPO_FLAG_DISK_CKSUM 0x4
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the store is in the process of shutting down. */
#define UMESTORE_DMON_REPO_FLAG_SHUTTING_DOWN 0x8
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the repository is of type RPP.
	See \ref umecfgrepositorytype "repository-type". */
#define UMESTORE_DMON_REPO_FLAG_RPP 0x10
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the store delays writes to disk.
	See \ref umecfgrepositorydiskwritedelay "repository-disk-write-delay". */
#define UMESTORE_DMON_REPO_FLAG_DELAY_WRITE 0x20

/*! \brief Message containing store disk statistics.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_DISK_STATS )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_disk_dmon_stat_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief The maximum size of the cache file, as configured by
		\ref umecfgrepositorydiskfilesizelimit "repository-disk-file-size-limit". */
	lbm_int64_t max_offset;
	/*! \brief Number of disk writes the store has submitted to the
		Operation System which have not yet completed. */
	lbm_int64_t num_ios_pending;
	/*! \brief Number of disk reads the store has submitted to the
		Operation System which have not yet completed. */
	lbm_int64_t num_read_ios_pending;
	/*! \brief The relative location on disk of where the message
		\ref umestore_repo_dmon_stat_msg_t::contig_sqn+1 will be written. */
	lbm_int64_t offset;
	/*! \brief The relative location on disk of the first message,
		\ref umestore_repo_dmon_stat_msg_t::trail_sqn. */
	lbm_int64_t start_offset;
	/*! \brief Identifying index number for the topic of this source,
		defined in an earlier message of type
		\ref LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG.
		See \ref umestore_topic_dmon_config_msg_t_stct::dmon_topic_idx. */
	lbm_uint32_t dmon_topic_idx;
	/*! \brief Registration ID used by the source to register. */
	lbm_uint32_t regid;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
} umestore_disk_dmon_stat_msg_t;
#define UMESTORE_DISK_DMON_STAT_MSG_T_SZ (offsetof(umestore_disk_dmon_stat_msg_t, store_idx) + sizeof(lbm_uint16_t))

/*! \brief Message containing store receiver statistics.
	<br>( \ref umestore_dmon_msg_hdr_t_stct::type == \ref LBM_UMESTORE_DMON_MPG_RCV_STATS )<br>
	Except where indicated, all fields of type `lbm_uintXX_t` should be
	byte-swapped if `hdr.magic` is equal to \ref LBM_UMESTORE_DMON_ANTIMAGIC. */
typedef struct umestore_rcv_dmon_stat_msg_t_stct {
	/*! \brief Standard header common to all messages. */
	umestore_dmon_msg_hdr_t hdr;
	/*! \brief Identifying index number for the topic of this source,
		defined in an earlier message of type
		\ref LBM_UMESTORE_DMON_MPG_STORE_TOPIC_CONFIG.
		See \ref umestore_topic_dmon_config_msg_t_stct::dmon_topic_idx. */
	lbm_uint32_t dmon_topic_idx;
	/*! \brief Registration ID used by the receiver to register. */
	lbm_uint32_t regid;
	/*! \brief The last message sequence number the receiver acknowledged. */
	lbm_uint32_t high_ack_sqn;
	/*! \brief Current store number, starting with 0, counting each
		\ref storeelement in the order configured. */
	lbm_uint16_t store_idx;
	/*! \brief Bit map of flags indicating various characteristics and
		current states of the store.  Possible bits:
		\ref UMESTORE_DMON_RCV_FLAG_FIRST_ACK,
		\ref UMESTORE_DMON_RCV_FLAG_OOD,
		\ref UMESTORE_DMON_RCV_FLAG_CAPABILITY_QC,
		\ref UMESTORE_DMON_RCV_FLAG_SHUTTING_DOWN,
		\ref UMESTORE_DMON_RCV_FLAG_DEREGISTERING,
		\ref UMESTORE_DMON_RCV_FLAG_RPP,
		\ref UMESTORE_DMON_RCV_FLAG_RPP,
		\ref UMESTORE_DMON_RCV_FLAG_RPP_IS_NON_BLOCKING,
		\ref UMESTORE_DMON_RCV_FLAG_KEEPALIVES_DISABLED. */
	lbm_uint16_t flags;
} umestore_rcv_dmon_stat_msg_t;
#define UMESTORE_RCV_DMON_STAT_MSG_T_SZ (offsetof(umestore_rcv_dmon_stat_msg_t, flags) + sizeof(lbm_uint16_t))

/*! \brief Bit for umestore_rcv_dmon_stat_msg_t_stct::flags
	indicating that the receiver has acked at least one message. */
#define UMESTORE_DMON_RCV_FLAG_FIRST_ACK 0x1
/*! \brief Bit for umestore_rcv_dmon_stat_msg_t_stct::flags
	which is reserved for future use. */
#define UMESTORE_DMON_RCV_FLAG_OOD 0x2
/*! \brief Bit for umestore_rcv_dmon_stat_msg_t_stct::flags
	indicating that the receiver can handle Quorum/Consensus. */
#define UMESTORE_DMON_RCV_FLAG_CAPABILITY_QC 0x4
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the store's internal registration for this receiver
	is in the process of shutting down. */
#define UMESTORE_DMON_RCV_FLAG_SHUTTING_DOWN 0x8
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that this receiver is in the process of deregistering. */
#define UMESTORE_DMON_RCV_FLAG_DEREGISTERING 0x10
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the receiver is participating in RPP. */
#define UMESTORE_DMON_RCV_FLAG_RPP 0x20
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the receiver is a non-blocking RPP receiver. */
#define UMESTORE_DMON_RCV_FLAG_RPP_IS_NON_BLOCKING 0x40
/*! \brief Bit for umestore_repo_dmon_stat_msg_t_stct::flags
	indicating that the receiver will generate proactive keepalives
	and that the store should not send keepalives. */
#define UMESTORE_DMON_RCV_FLAG_KEEPALIVES_DISABLED 0x80

#endif /* UME_DMON_MSGS_H */
