
	FILE: README.txt


This java sample formats UM statistics as JSON or XML configuration. 

NOTES:
===============================================================================
LBMMOND_JSON:
	lbmmond_json extends the lbmmon sample application with new options
	that create UM contexts and receivers to parse statistics generated
	by the umestored and the tnwgd UM daemons.
	
	The onReceive() callback for the receivers calls stat_parser(), a JNI
	function that:
	        - Combines the parsing utilities in the tnwgdmon.c (for tnwgd) and
	          umedmon.c (for umestored) sample applications
		(This approach effectively re-uses the existing parsers written in C.)
	        - Saves the statistics to a buffer in JSON/XML format.

	Two context are created; for each of the receivers. Attributes for each context
	can be set via XML configuration.
	
	A verbose options can be selected which prints the statistics in the
	format of lbmmon.java, tnwgdmon.c and umedmon.c
	
	The JSON/XML string is printed to stdout by default, or stored in a file
	specified on the command line.

LBMMOND_CMD:
	lbmmond_cmd uses the LBMContext.send() API to send requests for
	statistics to the umestored and tnwgd daemons.

	      -	lbmmond_cmd combines the functionality of the tnwgdcmd.c and umedcmd.c
		sample applications.

Note: Assumes JRE 1.6 or greater.
===============================================================================


Files: 
===============================================================================

Windows build and run examples:
-------------------------------
	bld.bat
	java_compile.bat
	java_run.example.bat
-------------------------------

Linux build and run examples:
-------------------------------
	bld.sh
	java_compile.sh
	java_run.example.sh
-------------------------------

Pre-built JNI shared libraries:
-------------------------------
	libdmon_parser.so
	dmon_parser.dll
-------------------------------

Example Store configuration to generate monitoring stats:
-------------------------------
	ump_store1.xml
	umestored.cfg
-------------------------------

Example DRO configuration that generates monitoring stats:
-------------------------------
	dro.xml
	TRD1.cfg
-------------------------------

Example Application configuration file:
-------------------------------
	sample_lbm.xml
-------------------------------

Deamon Monitoring Source configuration file:
-------------------------------
	tnwgd_dmonitor.cfg
	umestored_dmonitor.cfg
-------------------------------

Java helper libraries:
-------------------------------
	openmdx-kernel.jar
	log4j-1.2.9.jar
------------------------------

Java application source files:
-------------------------------
	lbmmond_cmd.java
	lbmmond_json.java
-------------------------------

JNI source file and dependencies:
-------------------------------
	monmodopts.h
	replgetopt.h
	lbmmond_json.h
	dmon_parser.c
-------------------------------

===============================================================================

Requirements:
-------------
To build and run the sample application, an install of UM6.11 (or greater version) is needed.
This will provide access to umedmonmsgs.h, tnwgdmonmsgs.h and lbm.h (needed to compile the JNI)
and the UMS_6.11.jar which is needed to build and run the java files.
A license file is also needed for the LBM sample programs.
-------------


Example:
-------------
These commands provide an example of how to test the application in LINUX:

1. Setup your PATH to point to the daemon executable(s) umestored and/or tnwgd 
	eg: # export PATH=$PATH:<my_install_dir>/UMP_6.11/Linux-glibc-2.5-x86_64/bin
2. Set your LD_LIBRARY_PATH to include the liblbm.so and liblbmstats_parser_jni.so
	eg: # export LD_LIBRARY_PATH=/lib:<my_install_dir>/UMP_6.11/Linux-glibc-2.5-x86_64/lib:.
3. Run the DRO and UMP daemons (configured to generate stats at intervals):
	# umestored ump_store1.xml -f
	# tnwgd dro.xml -f
4. Make sure both daemons are running:
	# ps -ef | grep -e tnwgd -e umestored
	root     23186     1  4 11:40 pts/1    00:00:07 umestored ump_store1.xml -f
	root     23220     1  0 11:42 pts/1    00:00:00 tnwgd dro.xml -f
	root     23244 20830  0 11:44 pts/1    00:00:00 grep -e tnwgd -e umestored
5. Run the lbmmond_json sample application:
	# export CP=.:UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar 
	# java -cp $CP lbmmond_json -x sample_lbm.xml  -s store_monitor_topic  -d drotopic
-------------
The commands for windows is very similar, however the PATH (not LD_LIBRARY_PATH)
needs to point to the compiled binaries. Please see example commands in the *.bat files


GENERATING MORE STATS:
----------------------------------------------------------------------

6. You can request statistics from the daemons using lbmmond_cmd. Example:
----------------------------------------------------------------------
	STORE:
	======
	# grep request_tcp_port umestored_dmonitor.cfg
	umestored_dmonitor.cfg:context request_tcp_port 15555
	[.. ..]
	# export CP=.:UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar 
	Type in command(s) delimited by ';' OR 'q;' to quit:
	# java -cp $CP  lbmmond_cmd -T"TCP:10.29.2.146:15555"
		"Example_Store_1" snap;"Example_Store_1" disk 5;
			Target: TCP:10.29.2.146:15555 Command: "Example_Store_1" disk 5;	
		Expect: 
			Response [TCP:10.29.2.146:48318[1]][0], 28 bytes: "Example_Store_1" snap - OK!
		q;
	
	DRO:
	======
	# grep request_tcp_port tnwgd_dmonitor.cfg
	tnwgd_dmonitor.cfg:context request_tcp_port 15551
	[.. ..]
	Type in command(s) delimited by ';' OR 'q;' to quit:
	# java -cp $CP  lbmmond_cmd -T"TCP:10.29.2.146:15551"
		snap;
		q;

7. Statistics can also be generated by sample applications:
----------------------------------------------------------------------
	SAMPLE APPLICATIONS:
	=======
	  TCP SRC: 	lbmsrc -P1000 mytopic --monitor-ctx=1
	LBTRM SRC: 	lbmsrc -P1000 -RM1g/1m mytopic --monitor-ctx=1
	LBTRU SRC: 	lbmsrc -P1000 -RU1g/1m mytopic --monitor-ctx=1
	
	RECEIVER: 	lbmrcv -v mytopic --monitor-ctx=1

----------------------------------------------------------------------

8. Cleanup:
	# pkill umestored; pkill tnwgd

======================================================================


KNOWN LIMITATIONS:
------------------

1. JSON serialization has some limitations: 
	+ 64-bit and hex values are not valid JSON and have to be represented as strings. 
	+ JSON control characters (such as ‘”’ or ‘\b’) are not excluded from UM configuration. 


