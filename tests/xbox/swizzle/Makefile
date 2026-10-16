CC=clang
CC=gcc
CFLAGS=-O2 -Wall -g

swizzle-test: swizzle-test.o swizzle-a.o
	$(CC) -o $@ $^

swizzle-test.o: swizzle-test.c

swizzle-a.o: swizzle.o
	objcopy \
		--redefine-sym swizzle_box=swizzle_box_A \
		--redefine-sym unswizzle_box=unswizzle_box_A \
		$< $@

swizzle.o: ../../../hw/xbox/nv2a/pgraph/swizzle.c
	$(CC) -o $@ $(CFLAGS) -c $<

%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f swizzle-test swizzle.o swizzle-a.o
