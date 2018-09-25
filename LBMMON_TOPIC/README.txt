lbmmon_topic.c: Example LBM monitoring application that includes callbacks for per-topic and wildcard receiver stats.

  ************************ ************************
  Enabling the undocumented monitor_interval (receiver) configuration in the application will enable
  contexts to publish statistics that include topic information in addition to the list of transport sessions.

  Enabling the undocumented monitor_interval (wildcard_receiver) configuration in the application
  will enable contexts to publish statistics that include the wildcard receiver pattern.

  Example:
  --------------
  [MONITORING] cat rcv_topic.cfg
        receiver monitor_interval 1
        wildcard_receiver monitor_interval 1

  [MONITORING] lbmwrcv -v -v -c rcv_topic.cfg mytopic

  Note that the monitor_interval (context) attribute for automatic monitoring will be forced on before
  the receiver scoped attribute takes effect.
  --------------

  BUILD:
  --------------
  Please refer to the documentation on how to build the sample applications. This is a linux build example:
  cc -g -L ./UMP_6.11/Linux-glibc-2.5-x86_64/lib  -I ./UMP_6.11/Linux-glibc-2.5-x86_64/include  -llbm -lm -lqpid-proton -o lbmmon_topic  lbmmon_topic.c
  --------------

  Example output:
  --------------
  Wildcard receiver statistics received from lbmwrcv at 10.29.2.146, process ID=a50, object ID=33d89c0, context instance=d0d683d873dc87e2, domain ID=0, sent Tue Sep 25 15:01:55 2018
        Pattern                                            : mytopic
        Type                                               : PCRE


  Receiver topic statistics received from umestored at 10.29.2.146, process ID=86d, object ID=3fa00e0, context instance=5c0169a25574da4d, domain ID=0, sent Tue Sep 25 14:24:53 2018
        Topic                                              : mytopic
        Source                                             : LBT-RU:10.29.2.146:14381:b40edd1e
        OTID                                               : 0100000000382d1edd0eb46aecd7219143a4927edd5c01000000000000000001
        Topic index                                        : 1793906465
  --------------

  ************************ ************************
