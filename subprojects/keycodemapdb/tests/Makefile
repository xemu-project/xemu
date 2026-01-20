TESTS := stdc stdc++ python2 python3 javascript rust

check: $(TESTS)
	@set -e; for fn in $(TESTS); do \
		./$$fn; \
		echo $$fn: OK; \
	done
	@echo Done.

GEN := ../tools/keymap-gen
DATA := ../data/keymaps.csv
SOURCES := $(GEN) $(DATA)

.DELETE_ON_ERROR:

stdc: stdc.c osx2win32.h osx2win32.c osx2win32_name.h osx2win32_name.c \
             osx2xkb.h osx2xkb.c osx2xkb_name.h osx2xkb_name.c \
             html2win32.h html2win32.c html2win32_name.h html2win32_name.c \
             osx.h osx.c osx_name.h osx_name.c
	$(CC) -Wall -o $@ $(filter %.c, $^)
osx2win32.c: $(SOURCES)
	$(GEN) code-map --lang stdc $(DATA) osx win32 > $@
osx2win32.h: $(SOURCES)
	$(GEN) code-map --lang stdc-header $(DATA) osx win32 > $@
osx2win32_name.c: $(SOURCES)
	$(GEN) name-map --lang stdc $(DATA) osx win32 > $@
osx2win32_name.h: $(SOURCES)
	$(GEN) name-map --lang stdc-header $(DATA) osx win32 > $@
osx2xkb.c: $(SOURCES)
	$(GEN) code-map --lang stdc $(DATA) osx xkb > $@
osx2xkb.h: $(SOURCES)
	$(GEN) code-map --lang stdc-header $(DATA) osx xkb > $@
osx2xkb_name.c: $(SOURCES)
	$(GEN) name-map --lang stdc $(DATA) osx xkb > $@
osx2xkb_name.h: $(SOURCES)
	$(GEN) name-map --lang stdc-header $(DATA) osx xkb > $@
html2win32.c: $(SOURCES)
	$(GEN) code-map --lang stdc $(DATA) html win32 > $@
html2win32.h: $(SOURCES)
	$(GEN) code-map --lang stdc-header $(DATA) html win32 > $@
html2win32_name.c: $(SOURCES)
	$(GEN) name-map --lang stdc $(DATA) html win32 > $@
html2win32_name.h: $(SOURCES)
	$(GEN) name-map --lang stdc-header $(DATA) html win32 > $@
osx.c: $(SOURCES)
	$(GEN) code-table --lang stdc $(DATA) osx > $@
osx.h: $(SOURCES)
	$(GEN) code-table --lang stdc-header $(DATA) osx > $@
osx_name.c: $(SOURCES)
	$(GEN) name-table --lang stdc $(DATA) osx > $@
osx_name.h: $(SOURCES)
	$(GEN) name-table --lang stdc-header $(DATA) osx > $@

stdc++: stdc++.cc osx2win32.hh osx2win32.cc osx2win32_name.hh osx2win32_name.cc \
             osx2xkb.hh osx2xkb.cc osx2xkb_name.hh osx2xkb_name.cc \
             html2win32.hh html2win32.cc html2win32_name.hh html2win32_name.cc \
             osx.hh osx.cc osx_name.hh osx_name.cc
	$(CXX) -Wall -std=c++11 -o $@ $(filter %.cc, $^)
osx2win32.cc: $(SOURCES)
	$(GEN) code-map --lang stdc++ $(DATA) osx win32 > $@
osx2win32.hh: $(SOURCES)
	$(GEN) code-map --lang stdc++-header $(DATA) osx win32 > $@
osx2win32_name.cc: $(SOURCES)
	$(GEN) name-map --lang stdc++ $(DATA) osx win32 > $@
osx2win32_name.hh: $(SOURCES)
	$(GEN) name-map --lang stdc++-header $(DATA) osx win32 > $@
osx2xkb.cc: $(SOURCES)
	$(GEN) code-map --lang stdc++ $(DATA) osx xkb > $@
osx2xkb.hh: $(SOURCES)
	$(GEN) code-map --lang stdc++-header $(DATA) osx xkb > $@
osx2xkb_name.cc: $(SOURCES)
	$(GEN) name-map --lang stdc++ $(DATA) osx xkb > $@
osx2xkb_name.hh: $(SOURCES)
	$(GEN) name-map --lang stdc++-header $(DATA) osx xkb > $@
html2win32.cc: $(SOURCES)
	$(GEN) code-map --lang stdc++ $(DATA) html win32 > $@
html2win32.hh: $(SOURCES)
	$(GEN) code-map --lang stdc++-header $(DATA) html win32 > $@
html2win32_name.cc: $(SOURCES)
	$(GEN) name-map --lang stdc++ $(DATA) html win32 > $@
html2win32_name.hh: $(SOURCES)
	$(GEN) name-map --lang stdc++-header $(DATA) html win32 > $@
osx.cc: $(SOURCES)
	$(GEN) code-table --lang stdc++ $(DATA) osx > $@
osx.hh: $(SOURCES)
	$(GEN) code-table --lang stdc++-header $(DATA) osx > $@
osx_name.cc: $(SOURCES)
	$(GEN) name-table --lang stdc++ $(DATA) osx > $@
osx_name.hh: $(SOURCES)
	$(GEN) name-table --lang stdc++-header $(DATA) osx > $@

python2: osx2win32.py osx2win32_name.py \
         osx2xkb.py osx2xkb_name.py \
         html2win32.py html2win32_name.py \
         osx.py osx_name.py
osx2win32.py: $(SOURCES)
	$(GEN) code-map --lang python2 $(DATA) osx win32 > $@
osx2win32_name.py: $(SOURCES)
	$(GEN) name-map --lang python2 $(DATA) osx win32 > $@
osx2xkb.py: $(SOURCES)
	$(GEN) code-map --lang python2 $(DATA) osx xkb > $@
osx2xkb_name.py: $(SOURCES)
	$(GEN) name-map --lang python2 $(DATA) osx xkb > $@
html2win32.py: $(SOURCES)
	$(GEN) code-map --lang python2 $(DATA) html win32 > $@
html2win32_name.py: $(SOURCES)
	$(GEN) name-map --lang python2 $(DATA) html win32 > $@
osx.py: $(SOURCES)
	$(GEN) code-table --lang python2 $(DATA) osx > $@
osx_name.py: $(SOURCES)
	$(GEN) name-table --lang python2 $(DATA) osx > $@

javascript: node_modules/babel-core \
            node_modules/babel-plugin-transform-es2015-modules-commonjs \
            osx2win32.js osx2win32_name.js \
            osx2xkb.js osx2xkb_name.js \
            html2win32.js html2win32_name.js \
            osx.js osx_name.js
node_modules/babel-core:
	npm install babel-core
node_modules/babel-plugin-transform-es2015-modules-commonjs:
	npm install babel-plugin-transform-es2015-modules-commonjs
osx2win32.js: $(SOURCES)
	$(GEN) code-map --lang js $(DATA) osx win32 > $@
osx2win32_name.js: $(SOURCES)
	$(GEN) name-map --lang js $(DATA) osx win32 > $@
osx2xkb.js: $(SOURCES)
	$(GEN) code-map --lang js $(DATA) osx xkb > $@
osx2xkb_name.js: $(SOURCES)
	$(GEN) name-map --lang js $(DATA) osx xkb > $@
html2win32.js: $(SOURCES)
	$(GEN) code-map --lang js $(DATA) html win32 > $@
html2win32_name.js: $(SOURCES)
	$(GEN) name-map --lang js $(DATA) html win32 > $@
osx.js: $(SOURCES)
	$(GEN) code-table --lang js $(DATA) osx > $@
osx_name.js: $(SOURCES)
	$(GEN) name-table --lang js $(DATA) osx > $@

rust: osx2win32.rs osx2win32_name.rs \
      osx2xkb.rs osx2xkb_name.rs \
      html2win32.rs html2win32_name.rs \
      osx.rs osx_name.rs
osx2win32.rs: $(SOURCES)
	$(GEN) code-map --lang rust $(DATA) osx win32 > $@
osx2win32_name.rs: $(SOURCES)
	$(GEN) name-map --lang rust $(DATA) osx win32 > $@
osx2xkb.rs: $(SOURCES)
	$(GEN) code-map --lang rust $(DATA) osx xkb > $@
osx2xkb_name.rs: $(SOURCES)
	$(GEN) name-map --lang rust $(DATA) osx xkb > $@
html2win32.rs: $(SOURCES)
	$(GEN) code-map --lang rust $(DATA) html win32 > $@
html2win32_name.rs: $(SOURCES)
	$(GEN) name-map --lang rust $(DATA) html win32 > $@
osx.rs: $(SOURCES)
	$(GEN) code-table --lang rust $(DATA) osx > $@
osx_name.rs: $(SOURCES)
	$(GEN) name-table --lang rust $(DATA) osx > $@

clean:
	rm -rf node_modules
	rm -f osx2win32.*
	rm -f osx2win32_name.*
	rm -f osx2xkb.*
	rm -f osx2xkb_name.*
	rm -f html2win32.*
	rm -f html2win32_name.*
	rm -f osx.*
	rm -f osx_name.*
	rm -f stdc stdc++
