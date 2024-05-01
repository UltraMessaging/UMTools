- This application is a linux utility for processing LBM Topic resolution(TR) packets (LBMR) from live traffic or a packet capture.

- Build and Run the application with the help functionality to view the current capabilities.

	Build Example :
	----------------
	gcc -g -pthread  -lm  -o TRsniffer TRsniffer.c  lbmotid.c TS_util.c

 	Command lines examples:
	---------------------
	# List the help menu
	[localhost TR]# ./TRsniffer -h

	# Listen to multicast group 224.9.52.101, dump stats every second and roll over logs every 10MB
	[localhost TR]#  ./TRsniffer -s1  -F10 -m224.9.52.101 -v 
	
	# Clear accumulated stats every 30 seconds
	[localhost TR]# ./TRsniffer -s1 -v -C 30
	
	# Read the dro_toxbones.pcap pcap file with full debug mask and accumulate stats
	[localhost TR]# ./TRsniffer  -r dro_toxbones.pcap   -lffffffff -s0 


 -  FILE HISTORY:

	v 0.1 :
		- listen to an lbmrd
	v 0.2 :
		- incorporate packet structure headers
	v 0.3 : 
		- read pcap data
		- set logging levels
		- accumulate sstats
		- Save output to rolling logs
	v 0.4.2 : 
		- Skip through contiguous 802.1q ethernet extended header
		- NCFs look like poison TMRs; added a limiter
	v 0.5 : 
		- Added '-f' flag to change 'LBMR packet found' logs to log level PRINT_LBMR (most default to PRINT_STAT).
		  Thus, supplying "-f -l 0x7" prints periodic stats without printing any per-packet logs.
		  Omitting "-f" leaves TRsniffer in its previous behaviour.
		- Compiled with -Wall and got rid of all the warnings.
		- Corrected 3 minor bugs.


 -   Known issues:

	+ Stats don't work when reading pcap files.
	+ -E option does not work 
	+ -r option: Out of order packets (especially when some are missing) can cause segfaults in reassembly.

