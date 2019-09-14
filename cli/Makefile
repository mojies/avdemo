plat ?= mips

ifeq ($(plat),mips)
include ../../../env.mk
CC = mipsel-linux-gcc
CFLAGS += -I$(DEPENDS_INCLUDE_DIR)
CFLAGS += -I$(COMMON_INCLUDE_DIR)
LDFLAGS += -L$(DEPENDS_LIB_DIR)
LDFLAGS += -L$(COMMON_LIB_DIR)
else
CC = gcc
endif

CFLAGS += -W -O2


LDFLAGS += -lpthread -lm

SRCS += ../tools/unixdomain_socket.c
SRCS += ../tools/cjson.c
SRCS += ../tools/package.c
SRCS += main.c



all: avDeamon.t
	echo "build done!"

avDeamon.t:
	$(CC) $(CFLAGS) -o $(@) $(SRCS) $(LDFLAGS)

clean:
	rm -rf avDeamon.t

install:all
	cp avDeamon.t /mnt/nfs/tools/



