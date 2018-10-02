This application can be used to dump the transport (XPORT) source string and the original source string from TIRs.

It can also flag and execute TR requests based on timeout criteria:
1)
"  -S, --src-tr-timeout=NUM  Execute TR-request if there is no SRC context TR activity after NUM secs\n"
    This options expects a configuration or environment that has sources constantly advertising TIR. 
    It is the lightest weight option and uses the "resolver_event_function" context callback to monitor TIRs.

2) 
"  -R, --rcv-eos='topic'     Execute TR-request if there is no heart-beat 'topic' BOS after the last EOS + configured sustain-phase TQR timeout \n"
    This options expects to receive messages from a heartbeat topic. 
    It creastes a receiver and if an EOS is declared on the transport, it waits the configured TQR sustain phase before issuing TR requests.

3)
"  -W, --wrcv-eos='pattern'  Execute TR-request if no heart-beat BOS after 'pattern' EOS + TQR timeout \n"
    This creates a wildcard receiver and behaves similar to the "-R" option. 

4) There is also a "-n" option that prints a warning if there is no response to queries. 


This is the help file:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
c:\>lbm_watchdog.exe -h
[.. ..]
c:\Program Files\Informatica\UMQ_6.11.1\Win2k-x86_64\bin\NEW>lbm_watchdog.exe -v -S5 -D --rcv-eos=mytopic  -hg
        chk: **********INFO! '-D' option disables TR Requests !********
UMQ 6.11.1 [UMP-6.11.1] [UMQ-6.11.1] [64-bit] Build: Apr 30 2018, 11:34:01 ( DEBUG license LBT-RM LBT-RU LBT-IPC LBT-SMX BROKER ) WC[PCRE 6.7 04-Jul-2006,
appcb] HRT[QueryPerformanceCounter()]
Usage: lbm_watchdog.exe [at least one option]
Available options:
  -c, --config=FILE     Use LBM configuration file FILE.
                        Multiple config files are allowed.
                        Example:  '-c file1.cfg -c file2.cfg'
  -a, --adverts         Request Advertisements (default)
  -q, --queries         Request Queries (default)
  -w, --wildcard        Request Wildcard Queries (default)
  -A, --ctx-ads         Request Context Advertisements (default)
  -Q, --ctx-queries     Request Context Queries (default)
  -I, --gw-interest     Request Gateway Interest (default)
  -i, --interval=NUM    Interval between request (default)
  -d, --duration=NUM    Minimum duration of requests
  -n, --no_resp=NUM     Warn if there is no response to NUM queries (only valid with -R option, default=10)
  -L, --linger=NUM      Linger for NUM seconds after the set of TR requests are sent (default-10)
  -R, --rcv-eos='topic'     Execute TR-request if there is no heart-beat 'topic' BOS after the last EOS + configured sustain-phase TQR timeout
  -M, --msg-timeout=NUM     Execute TR-request if receiver does not get a message with NUM timeout secs
  -S, --src-tr-timeout=NUM  Execute TR-request if there is no SRC context TR activity after NUM secs
  -W, --wrcv-eos='pattern'  Execute TR-request if no heart-beat BOS after 'pattern' EOS + TQR timeout
  -D, --disable-tr-request  Disable TR-requests from being executed
  -v, --verbose             Be verbose about incoming messages (-v -v  = be very verbose)
  -Y, --xml_appname        specify the application name
  -Z, --xml_config=FILE    Use LBM configuration XML FILE.


Examples:
1) This will check for EOS on the "mytopic" topic. It will also print the data messages received and a sequence number. 
lbm_watchdog.exe -v -D --rcv-eos=mytopic  -v

2) This will command will print a verbose information for each TIR: 
c:\lbm_watchdog.exe -v -v -S5 -D

3) Execute TR requests (every 10 seconds) if there is no BOS after receiver mytopic EOS + configured sustain interval, using myconfig.txt configuration file.
c:\lbm_watchdog  -Rmytopic -c myconfig.txt

4) Execute TR requests after EOS on topics that fit PCRE wildcard "mypattern" with very verbose output
c:\lbm_watchdog  -Wmypattern -c myconfig.txt -v -v 

5) Execute TR requests if there is a break in TIRs for 5 seconds:
c:\lbm_watchdog  -S5 -c myconfig.txt

