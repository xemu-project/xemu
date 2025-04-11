#!/usr/bin/env python3
import tomli
with open('config_out.toml') as f:
	t = tomli.loads(f.read())
	assert(t['company']['products'][0]['suppliers'][-1] == "Fred's Apples LLC")
	assert(t['company']['products'][1]['price'] == 995.75)
