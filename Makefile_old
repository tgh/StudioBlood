# Copyright © 2009 Tyler Hayes
# ALL RIGHTS RESERVED
#
# [This program is licensed under the GPL version 3 or later.]
# Please see the file COPYING in the source
# distribution of this software for license terms.
#
# Thanks to Tom Szilagyi (http://tap-plugins.sourceforge.net/)
# for his Makefile as an example for me.  Also special thanks
# to Bart Massey (http://web.cecs.pdx.edu/~bart/) for his help
# in getting the correct flags for creating the shared object.
#
# NOTE: since LADSPA_PATH varies depending on your distro,
# you must change the LADSPA_PATH variable and UNINSTALL
# variable in this Makefile to match your LADSPA_PATH
# environment variable!

#-----------------------------------------------------

CC = gcc
CFLAGS	= -Wall -O3 -fPIC
LDFLAGS = -nostartfiles -shared -Wl,-Bsymbolic
LADSPA_PATH = /usr/lib/ladspa      # change these 2 variables to match
UNINSTALL = /usr/lib/ladspa/sb_*   # your LADSPA_PATH environment
                                   # variable (type 'echo $LADSPA_PATH
                                   # at your shell prompt)
PLUGINS	=	sb_adt.so \
		sb_esreveR.so \
		sb_kite.so \
		sb_revolution.so \
		sb_ringer.so

# ----------------------------------------------------

all: $(PLUGINS)

# --- .so files ---

sb_adt.so: sb_adt.o
	$(CC) $(LDFLAGS) -o sb_adt.so sb_adt.o

sb_esreveR.so: sb_esreveR.o xorgens.o
	$(CC) $(LDFLAGS) -o sb_esreveR.so sb_esreveR.o xorgens.o

sb_kite.so: sb_kite.o xorgens.o
	$(CC) $(LDFLAGS) -o sb_kite.so sb_kite.o xorgens.o

sb_revolution.so: sb_revolution.o
	$(CC) $(LDFLAGS) -o sb_revolution.so sb_revolution.o

sb_ringer.so: sb_ringer.o
	$(CC) $(LDFLAGS) -o sb_ringer.so sb_ringer.o

# --- .o files ---

sb_adt.o: ./ADT/sb_adt.c ladspa.h
	$(CC) $(CFLAGS) -c ./ADT/sb_adt.c

sb_esreveR.o: ./esreveR/sb_esreveR.c xorgens.c ladspa.h xorgens.h
	$(CC) $(CFLAGS) -c ./esreveR/sb_esreveR.c
	$(CC) $(CFLAGS) -c xorgens.c

sb_kite.o: ./Kite/sb_kite.c xorgens.c ladspa.h xorgens.h
	$(CC) $(CFLAGS) -c ./Kite/sb_kite.c
	$(CC) $(CFLAGS) -c xorgens.c

sb_revolution.o: ./Revolution/sb_revolution.c ladspa.h
	$(CC) $(CFLAGS) -c ./Revolution/sb_revolution.c

sb_ringer.o: ./Ringer/sb_ringer.c ladspa.h
	$(CC) $(CFLAGS) -c ./Ringer/sb_ringer.c

# ----------------------------------------------------

install: $(PLUGINS)
	cp sb_*.so $(LADSPA_PATH)

uninstall:
	rm -f $(UNINSTALL)

clean:
	rm -f *.o *.so
