CC=gcc
CFLAGS=-Wall -g --coverage

LIB=rlc_entity.o rlc_entity_am.o rlc_pdu.o rlc_sdu.o

tests:
	@./run_tests.sh

all: clean_run $(TEST).run

%.run: test
	./test | grep ^TEST > $(TEST).run
	diff -q $(TEST).txt $(TEST).run

test: test.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^

%.o: ../%.c
	$(CC) $(CFLAGS) -I.. -c -o $@ $<

test.o: test.c
	$(CC) $(CFLAGS) -c -o $@ $< -DTEST='"$(TEST).h"'

clean_run:
	rm -f $(TEST).run test test.o

clean:
	rm -f *.o test *.run *.gcov *.gcda *.gcno
