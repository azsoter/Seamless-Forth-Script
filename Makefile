CC=gcc
# CFLAGS+= -O3 -Itest-app -Iforth -MMD  -Xlinker -Map=test.map
CFLAGS+= -O3 -Itest-app -Iforth -MMD
# LDFLAGS=-pthread

OBJ = main.o forth_blk_io.o forth.o forth_search.o forth_configurable.o forth_stdio.o forth_blocks.o forth_locals.o
OBJ_CURSES = main_test_curses.o forth_blk_io.o forth.o forth_search.o forth_configurable.o forth_blocks.o forth_locals.o
default: test blk

-include $(OBJ:%.o=%.d)

%.o: forth/%.c
	$(CC) $(CFLAGS) $< -c -o $@

%.o: test-app/%.c
	$(CC) $(CFLAGS) $< -c -o $@

blk:
	mkdir -p blk

test: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o test $(LDFLAGS)

test_curses: $(OBJ_CURSES)
	$(CC) $(CFLAGS) $(OBJ_CURSES) -lncurses $(LDFLAGS)  -o test_curses

run-tests:	test quick-tests.txt
	./test <quick-tests.txt >results.txt
	./test <local-tests.txt >>results.txt

.PHONY: clean

clean:
	$(RM) *.o *.d test test.map test_curses test_curses.map results.txt



	