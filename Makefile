CPPFLAGS +=
CFLAGS += -O2 -g -Wall #-Wsign-compare -Wfloat-equal -Wformat-security #-Werror
LDFLAGS +=
#LDLIBS += -lpthread -lrt


all: memtool

clean:
	-rm -f memtool

install:

.PHONY: all install clean
