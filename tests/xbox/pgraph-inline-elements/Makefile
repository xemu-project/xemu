# SPDX-License-Identifier: GPL-2.0-or-later

CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror

pgraph-inline-elements-test: pgraph-inline-elements-test.c
	$(CC) $(CFLAGS) -I../../.. -o $@ $<

.PHONY: check clean

check: pgraph-inline-elements-test
	./pgraph-inline-elements-test

clean:
	rm -f pgraph-inline-elements-test
