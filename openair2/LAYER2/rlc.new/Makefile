CC=gcc
CFLAGS=-g -Wall

OBJS=rlc_entity.o rlc_entity_am.o rlc_pdu.o main.o

test_am : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o test_am
