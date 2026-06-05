# Makefile for store_auto_maint
#
# Build requirements:
#   Linux: gcc, standard C library (no external dependencies)
#   Windows: Visual Studio or MinGW (see cross-compile notes below)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lpthread

TARGET = store_auto_maint
SOURCES = store_auto_maint.c xml_config_parser.c
HEADERS = xml_config_parser.h
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

store_auto_maint.o: store_auto_maint.c xml_config_parser.h
	$(CC) $(CFLAGS) -c $<

xml_config_parser.o: xml_config_parser.c xml_config_parser.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS) $(TARGET)

# Test the XML parser against a sample config
test: $(TARGET)
	@echo "=== Validate-only test ==="
	./$(TARGET) -x test_config.xml -V

# ---------------------------------------------------------------
# Cross-compile notes for Windows (MinGW):
#   CC = x86_64-w64-mingw32-gcc
#   CFLAGS += -D_WIN32
#   LDFLAGS = -lws2_32
#   Target: store_auto_maint.exe
#
# Native Windows build (Visual Studio):
#   cl /O2 /W3 store_auto_maint.c xml_config_parser.c /Fe:store_auto_maint.exe
# ---------------------------------------------------------------

# ---------------------------------------------------------------
# Building umesnaprepo (requires UMP/LBM SDK)
#
# umesnaprepo is a UMP tool shipped with the UMP SDK. If you need
# to build it from source, adjust UMP_DIR to match your SDK path
# and run the command below (not integrated into this Makefile
# because the SDK path and library set are site-specific).
#
# Example (UMP 6.17, Linux x86_64):
#
#   UMP_DIR = UMP_6.17/Linux-glibc-2.17-x86_64
#
#   gcc umesnaprepo.c -o umesnaprepo \
#       -D_FILE_OFFSET_BITS=64 -Wno-long-long -fno-strict-aliasing \
#       -D_REENTRANT -g -O3 \
#       -I$(UMP_DIR)/include \
#       -I$(UMP_DIR)/include/lbm \
#       -I. \
#       -L$(UMP_DIR)/lib \
#       -lumestorelib -llbm -llbmsdm -llbmutl -lrsock \
#       -lqpid-proton -lstdc++ -lpthread -lcrypto -lssl \
#       -lrt -lm -ldl -lsmartheap_smp64 -lprotobuf-c
#
# Example (Windows, Visual Studio, UMP 6.17 Win2k-x86_64):
#
#   set UMQ=C:\path\to\UMQ_6.17\Win2k-x86_64
#
#   cl -DWIN32_LEAN_AND_MEAN -DWIN32_EXTRA_LEAN -DHAVE_CONFIG_H ^
#       -I"%UMQ%\include" ^
#       -I"%UMQ%\include\lbm" ^
#       -I. ^
#       /Ob1 /Oi /Ot /O2 /GL /Z7 -Fd.\ -c umesnaprepo.c
#
#   Note 1: Two -I flags are required:
#     -I"%UMQ%\include"      for top-level UME/LBM headers
#     -I"%UMQ%\include\lbm"  so that #include "gen/um_mon_attributes.pb-c.h"
#                             inside lbmmon.h resolves via the gen\ subdirectory
#   Note 2: Do NOT pass -D_FILE_OFFSET_BITS=64 on Windows (Linux-only macro).
#   Note 3: Use the Windows SDK (Win2k-x86_64), not the Linux-x86_64 headers.
#
# ---------------------------------------------------------------
