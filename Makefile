PREFIX		?=/usr/local
INSTALLDIR	= $(DESTDIR)$(PREFIX)/bin

CC		?= gcc
CFLAGS		?= -O3 -Wall -Wextra
CFLAGS 		+= -std=gnu99
INSTFLAGS	= -m 0755

INSTFLAGS += -D

OBJS = wifidog.o pcap.o byteops.o ieee80211.o hashops.o strings.o

all: wifidog

wifidog: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# $(CFLAGS) $(CPPFLAGS) wifidog.c $(LDFLAGS)

install: wifidog
	install $(INSTFLAGS) wifidog $(INSTALLDIR)/wifidog

clean:
	rm -f wifidog
	rm -f $(OBJS)
	rm -f *~
	rm -f wifidog.pcap


uninstall:
	rm -f $(INSTALLDIR)/wifidog

