REM #Example windows compile script for JNI shared library 
cl /I "C:\Program Files\Java\jdk1.8.0_25\include" /I "C:\Program Files\Java\jdk1.8.0_25\include\win32"  /I "C:\Program Files\Informatica\UMQ_6.11.1\Win2k-x86_64\include"  /LD  dmon_parser.c  /link   Ws2_32.lib 
