cd "c:\Program Files\Informatica\UMQ_6.11.1\doc\example"
cl /I..\..\include /Fd.\ /Z7 lbmotid.c  getopt.c lbm_watchdog.c /link ..\..\lib\lbm.lib Ws2_32.lib /DEBUG /out:lbm_watchdog.exe


