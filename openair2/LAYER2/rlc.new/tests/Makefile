CC=gcc
CFLAGS=-Wall -g -I..

LIB=../rlc_entity.o ../rlc_entity_am.o ../rlc_pdu.o ../rlc_sdu.o

all: clean_run test1.run

%.run: %
	./$< | grep ^TEST > $<.run
	diff -q $<.txt $<.run

test1: test1.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean_run:
	rm -f *.run

clean:
	rm -f *.o test1 *.run
	cd .. && make clean
