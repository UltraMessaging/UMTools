#!/bin/sh
# Build umesnaprepo (Linux / gcc).
#
# Set UMP_DIR to your UMP SDK root before running, e.g.:
#   export UMP_DIR=/opt/UMP_6.17/Linux-glibc-2.17-x86_64
#   ./bld_umesnaprepo.sh
set -e

: "${UMP_DIR:?Set UMP_DIR to the UMP SDK root before running (e.g. /opt/UMP_6.17/Linux-glibc-2.17-x86_64)}"

gcc umesnaprepo.c -o umesnaprepo \
    -D_FILE_OFFSET_BITS=64 -Wno-long-long -fno-strict-aliasing \
    -D_REENTRANT -g -O3 \
    -I"$UMP_DIR/include" \
    -I"$UMP_DIR/include/lbm" \
    -I. \
    -L"$UMP_DIR/lib" \
    -Wl,--as-needed \
    -lumestorelib -llbm -llbmsdm -llbmutl -lrsock \
    -lqpid-proton -lstdc++ -lpthread -lcrypto -lssl \
    -lrt -lm -ldl -lsmartheap_smp64 -lprotobuf-c \
    -no-pie
