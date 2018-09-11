#linux JNI build command example 
gcc  -shared  -fPIC  -L/home/ibu/Desktop/UMP_6.11/Linux-glibc-2.5-x86_64/lib/ -I/home/ibu/Desktop/UMP_6.11/Linux-glibc-2.5-x86_64/include -I.  -I /usr/lib/jvm/java-1.7.0/include -I /usr/lib/jvm/java-1.7.0/include/linux  -llbm -lm  -o libdmon_parser.so  dmon_parser.c

