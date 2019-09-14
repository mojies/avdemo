
# plat ?= mips

ifeq ($(plat),mips)
# include ../../../env.mk
CC = mipsel-linux-gcc
CFLAGS += -I$(DEPENDS_INCLUDE_DIR)
CFLAGS += -I$(COMMON_INCLUDE_DIR)
LDFLAGS += -L$(DEPENDS_LIB_DIR)
LDFLAGS += -L$(COMMON_LIB_DIR)
else
CC = gcc
# include ../../env.mk
CFLAGS += -I$(LIBAV_INCLUDE_DIR)
LDFLAGS += -L$(LIBAV_LIB_DIR)
endif

CFLAGS += -W -O2
# CFLAGS += -O2

LDFLAGS += -lpthread
LDFLAGS += -lm
LDFLAGS += -lavcodec -lavformat -lavdevice -lavfilter -lavresample -lavutil -lswscale
LDFLAGS += -lssl -lcrypto -lz

SRCS += ../tools/unixdomain_socket.c
SRCS += ../tools/cjson.c
SRCS += ../tools/linklist.c
SRCS += ../tools/package.c
SRCS += ../tools/params.c
SRCS += ../tools/tools.c
SRCS += ../tools/Debug.c
SRCS += devices/audio_device_oss.c
# SRCS += devices/oss.c
SRCS += player.c
SRCS += msgCenter.c
SRCS += task.c
SRCS += main.c



all: AVDeamon
	echo "build done!"

AVDeamon:
	$(CC) $(CFLAGS) -o $(@) $(SRCS) $(LDFLAGS)

clean:
	rm -rf AVDeamon

install:all
	cp AVDeamon /mnt/nfs/tools/

run:
	LD_LIBRARY_PATH=../../libav/__install/lib ./AVDeamon
