INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc
LDFLAGS = -lpthread
CDEFS=
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS) -Wall -g
LIBS= 

HFILES= 
CFILES= pthread.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all: sum_threads.c
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)


clean:
	-rm -f *.o *.d
	-rm -f perfmon pthread

distclean:
	-rm -f *.o *.d
	-rm -f pthread

pthread: $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
