#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for the v1.79 page-table snapshot memory-map path."""
from __future__ import annotations
import argparse, pathlib, random
from v287_source_test_utils import read_memory_tools_implementation

PAGE=0x1000

def expand_ranges(ranges, ram_size):
    out=[]
    for v,p,n in sorted(ranges):
        off=0
        while off<n:
            va=v+off; pa=p+off
            if va>=0x100000000 or pa>=ram_size: break
            out.append((va, pa & ~(PAGE-1)))
            off += PAGE
    # same sort/unique identity as the C++ helper
    return sorted(set(out))

def legacy_from_pages(pages, ram_size):
    return sorted((v,p) for v,p in pages.items() if p < ram_size)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--root',default='.')
    root=pathlib.Path(ap.parse_args().root).resolve()
    c=(root/'ui/xui/debug-tools/cheat-engine-memory.c').read_text()
    h=(root/'ui/xui/debug-tools/cheat-engine-memory.h').read_text()
    cc=read_memory_tools_implementation(root / "ui/xui/debug-tools")
    for needle in (
        'cpu_get_memory_mapping(cpu, &list, &local_err)',
        'xemu_cheat_collect_ram_virtual_mappings',
        'xemu_cheat_free_virtual_mappings',
    ):
        assert needle in c or needle in h, needle
    assert 'CollectRamVirtualPages' in cc
    assert 'legacy page scan' in cc
    assert 'page-table snapshot' in cc
    # Randomized semantic equivalence: contiguous snapshot ranges must expand to
    # exactly the same page pairs as the old per-virtual-page probe model.
    rnd=random.Random(0x179)
    ram=128*1024*1024
    for _ in range(2000):
        pages={}
        ranges=[]
        v=(rnd.randrange(0, 0x100000) * PAGE) & 0xffffffff
        p=rnd.randrange(0, ram//PAGE)*PAGE
        for _ in range(rnd.randrange(1,80)):
            run=rnd.randrange(1,24)
            # avoid wrap for this synthetic run
            if v + run*PAGE >= 0x100000000 or p + run*PAGE > ram: break
            ranges.append((v,p,run*PAGE))
            for i in range(run): pages[v+i*PAGE]=p+i*PAGE
            v += (run+rnd.randrange(0,7))*PAGE
            p = rnd.randrange(0,ram//PAGE)*PAGE
        assert expand_ranges(ranges,ram)==legacy_from_pages(pages,ram)
    print('PASS: v1.79 page-table snapshot map expansion matches legacy page semantics')
    return 0
if __name__=='__main__': raise SystemExit(main())
