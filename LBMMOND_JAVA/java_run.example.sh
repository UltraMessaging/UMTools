#Linux example to run lbmmond_json and lbmmond_cmd sample applications 
java -cp .:UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar  lbmmond_json -h 
java -cp .:UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar  lbmmond_cmd -h 

java -cp .:UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar  lbmmond_json -x sample_lbm.xml  -s store_monitor_topic  -d drotopic -o outfile.json -v 

