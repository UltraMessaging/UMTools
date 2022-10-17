# UMTools

Special-purpose tools for Ultra Messaging

# FILES:

1. ummon.c		-> lbmmon.c clone with output written in CSV format.
1. umpub.c		-> UMP publisher for capacity testing.
1. umsub.c		-> UMP receiver for capacity testing.
1. murmur2_test.c	-> Reads a set of topic names and gives the number and
lengths of the resulting hash chains using UM's default
[resolver hash function](https://ultramessaging.github.io/currdoc/doc/Config/grpudpbasedresolveroperation.html#resolverstringhashfunctioncontext)
"murmur2".

# DIRECTORIES:

1. LBMMON_JSON(java)	-> Combines lbmmon.json with umedmon.c and tnwgdmon.c parsers (compiled into a JNI function) to parse statistics
			   from UM applications and Daemons; with output formatted as JSON by default.
			-> Also includes lbmmond_cmd which uses the LBMContext.send() API to send requests for statistics to the umestored and tnwgd daemons.
1. LBMMON_TOPIC(c)	-> Adds topic-level and wildcard statistics to the "lbmmon.c" example app. (The stock 6.15 version does this.)
1. LBMMON_SRS(c,java)	-> Adds SRS statistics to the "lbmmon.c" and "lbmmon.java" example apps.
1. TRSNIFFER(c)		-> Utility for parsing Topic Resolution streams 
1. WATCHDOG(c)		-> Checks for absence of TR packets or a topic stream and has the option of issuing TR requests to restart topic resolution.

The mtools tool set has moved to: https://github.com/UltraMessaging/mtools
