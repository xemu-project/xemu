.PHONY: all
all: example

tomlplusplus:
	git clone https://github.com/marzer/tomlplusplus

config.h: config_spec.yml
	python ../gen_config.py config_spec.yml config.h

example: main.cpp config.h tomlplusplus
	g++ -o $@ -g -std=c++17 -Itomlplusplus -I.. main.cpp
