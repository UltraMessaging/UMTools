#linux java build command example

export CP=.:UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar:UMSSDM_6.11.jar:UMSPDM_6.11.jar
export LD_LIBRARY_PATH=/lib:/home/ibu/Desktop/DEV/UMP_6.11/Linux-glibc-2.5-x86_64/lib:.
javac -cp UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar  lbmmond_json.java
javac -cp UMS_6.11.jar:openmdx-kernel.jar:log4j-1.2.9.jar  lbmmond_cmd.java
