

plat ?= mips

ifeq ($(plat),mips)
include ../../../env.mk
CC = mipsel-linux-gcc
CFLAGS += -I$(DEPENDS_INCLUDE_DIR)
CFLAGS += -I$(COMMON_INCLUDE_DIR)
LDFLAGS += -L$(DEPENDS_LIB_DIR)
LDFLAGS += -L$(COMMON_LIB_DIR)
else
LIBDIR = /home/mojies/CC/C/tools/libav-12/__install/lib
INCLUDEDIR = /home/mojies/CC/C/tools/libav-12/__install/include
CC = gcc
CFLAGS += -I$(INCLUDEDIR)
LDFLAGS += -L$(LIBDIR)
endif

CFLAGS += -W -O2
# CFLAGS += -O2

LDFLAGS += -lpthread
LDFLAGS += -lm
LDFLAGS += -lavcodec -lavformat -lavdevice -lavfilter -lavresample -lavutil -lswscale
LDFLAGS += -lssl -lcrypto -lz

SRCS += transcode_aac.c



all: Taac
	echo "build done!"

Taac:
	$(CC) $(CFLAGS) -o $(@) $(SRCS) $(LDFLAGS)

clean:
	rm -rf Taac

install:all
	cp Taac /mnt/nfs/tools/

run:
	LD_LIBRARY_PATH=$(LIBDIR) ./Taac /mnt/nfs/audio/test1.mp3 /tmp/test1.pcm
