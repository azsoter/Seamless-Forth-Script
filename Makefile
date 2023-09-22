CC=gcc
# CFLAGS+= -O3 -Itest-app -Iforth -MMD  -Xlinker -Map=test.map
CFLAGS+= -O3 -Itest-app -Iforth -MMD
# LDFLAGS=-pthread

OBJ = main.o forth.o forth_search.o forth_stdio.o

default: test

-include $(OBJ:%.o=%.d)

%.o: forth/%.c
	$(CC) $(CFLAGS) $< -c -o $@

%.o: test-app/%.c
	$(CC) $(CFLAGS) $< -c -o $@

test: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o test $(LDFLAGS)

.PHONY: clean

clean:
	$(RM) *.o *.d test test.map


	