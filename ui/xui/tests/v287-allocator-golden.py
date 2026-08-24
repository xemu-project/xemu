#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Compile the real Type-F free-list helpers and compare them to a reference model."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile
from v287_source_test_utils import extract_function


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--compiler", required=True)
    args = ap.parse_args()
    root = pathlib.Path(args.root).resolve()
    source = root / "ui/xui/debug-tools/external-code-memory.c"
    text = source.read_text(encoding="utf-8")

    markers = [
        "static uint32_t xemu_ext_align_size",
        "static int xemu_ext_find_first_fit",
        "static void xemu_ext_consume_free_block",
        "static int xemu_ext_find_free_insert",
        "static void xemu_ext_insert_and_coalesce",
    ]
    bodies = "\n\n".join(extract_function(text, marker) for marker in markers)

    generated = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

typedef unsigned guint;
typedef struct GArray { unsigned char *data; guint len; guint cap; size_t elem; } GArray;
static GArray *g_array_new(bool, bool, size_t e) {
    GArray *a = static_cast<GArray *>(std::calloc(1, sizeof(*a)));
    if (a) a->elem = e;
    return a;
}
static void g_array_set_size_impl(GArray *a, guint n) {
    if (n > a->cap) {
        guint next = a->cap ? a->cap : 4;
        while (next < n) next *= 2;
        void *p = std::realloc(a->data, static_cast<size_t>(next) * a->elem);
        if (!p) std::abort();
        a->data = static_cast<unsigned char *>(p);
        a->cap = next;
    }
    a->len = n;
}
#define g_array_set_size(a,n) g_array_set_size_impl((a),(n))
#define g_array_index(a,t,i) (((t*)((a)->data))[(i)])
#define g_array_append_val(a,v) do { guint _n=(a)->len; g_array_set_size((a),_n+1); g_array_index((a),decltype(v),_n)=(v); } while (0)
#define g_array_insert_val(a,i,v) do { guint _i=(i), _n=(a)->len; g_array_set_size((a),_n+1); std::memmove((a)->data+(_i+1)*(a)->elem,(a)->data+_i*(a)->elem,static_cast<size_t>(_n-_i)*(a)->elem); g_array_index((a),decltype(v),_i)=(v); } while (0)
static void g_array_remove_index(GArray *a, guint i) {
    std::memmove(a->data + static_cast<size_t>(i) * a->elem,
                 a->data + static_cast<size_t>(i + 1) * a->elem,
                 static_cast<size_t>(a->len - i - 1) * a->elem);
    --a->len;
}

#define XEMU_EXT_CODE_ALIGNMENT 0x00000010u
typedef struct XemuExtFreeBlock { uint32_t offset; uint32_t size; } XemuExtFreeBlock;

__BODIES__

struct RefBlock { uint32_t offset; uint32_t size; };
static uint32_t rng_state = 0xC0DEF00Du;
static uint32_t rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }

static bool same(const GArray *a, const std::vector<RefBlock> &r) {
    if (a->len != r.size()) return false;
    for (guint i=0;i<a->len;++i) {
        const auto &b = g_array_index(a, XemuExtFreeBlock, i);
        if (b.offset != r[i].offset || b.size != r[i].size) return false;
    }
    return true;
}
static bool ref_first(const std::vector<RefBlock>& r, uint32_t size, size_t &idx, uint32_t &off) {
    for (size_t i=0;i<r.size();++i) if (r[i].size >= size) { idx=i; off=r[i].offset; return true; }
    return false;
}
static void ref_consume(std::vector<RefBlock>& r, size_t idx, uint32_t size) {
    if (r[idx].size == size) r.erase(r.begin()+static_cast<long>(idx));
    else { r[idx].offset += size; r[idx].size -= size; }
}
static bool ref_insert_pos(const std::vector<RefBlock>& r, uint32_t off, uint32_t size, size_t &pos) {
    uint64_t end=static_cast<uint64_t>(off)+size; pos=0;
    for (size_t i=0;i<r.size();++i) {
        uint64_t bend=static_cast<uint64_t>(r[i].offset)+r[i].size;
        if (static_cast<uint64_t>(off)<bend && end>r[i].offset) return false;
        if (r[i].offset<off) pos=i+1;
    }
    return true;
}
static void ref_insert(std::vector<RefBlock>& r, size_t pos, uint32_t off, uint32_t size) {
    r.insert(r.begin()+static_cast<long>(pos), {off,size});
    if (pos>0 && static_cast<uint64_t>(r[pos-1].offset)+r[pos-1].size==r[pos].offset) {
        r[pos-1].size += r[pos].size; r.erase(r.begin()+static_cast<long>(pos)); --pos;
    }
    if (pos+1<r.size() && static_cast<uint64_t>(r[pos].offset)+r[pos].size==r[pos+1].offset) {
        r[pos].size += r[pos+1].size; r.erase(r.begin()+static_cast<long>(pos+1));
    }
}
struct Alloc { uint32_t off; uint32_t size; };
int main() {
    const uint32_t arena=0x000F0000u;
    for (uint32_t n=0;n<0x110000u;n+=37u) {
        uint32_t expected = (n==0 || n>arena) ? 0u : (n+15u)&~15u;
        if (xemu_ext_align_size(n, arena) != expected) return 10;
    }
    GArray *a=g_array_new(false,false,sizeof(XemuExtFreeBlock));
    XemuExtFreeBlock whole={0,arena}; g_array_append_val(a,whole);
    std::vector<RefBlock> ref={{0,arena}}; std::vector<Alloc> live;
    for (int step=0;step<1000000;++step) {
        bool do_free=!live.empty() && (rnd()%100)<43;
        if (!do_free) {
            uint32_t requested=(rnd()%0x9000u)+1u;
            uint32_t size=xemu_ext_align_size(requested,arena);
            guint ai=0; uint32_t ao=0; size_t ri=0; uint32_t ro=0;
            bool ag=xemu_ext_find_first_fit(a,size,&ai,&ao)!=0;
            bool rg=ref_first(ref,size,ri,ro);
            if (ag!=rg || (ag && (ai!=ri || ao!=ro))) return 20;
            if (ag) { xemu_ext_consume_free_block(a,ai,size); ref_consume(ref,ri,size); live.push_back({ao,size}); }
        } else {
            size_t pick=rnd()%live.size(); Alloc v=live[pick];
            guint ap=0; size_t rp=0;
            bool ag=xemu_ext_find_free_insert(a,v.off,v.size,&ap)!=0;
            bool rg=ref_insert_pos(ref,v.off,v.size,rp);
            if (!ag || !rg || ap!=rp) return 30;
            xemu_ext_insert_and_coalesce(a,ap,v.off,v.size); ref_insert(ref,rp,v.off,v.size);
            live[pick]=live.back(); live.pop_back();
        }
        if ((step%97)==0 && !ref.empty()) {
            const RefBlock v=ref[rnd()%ref.size()]; guint ap=0; size_t rp=0;
            bool ag=xemu_ext_find_free_insert(a,v.offset,v.size,&ap)!=0;
            bool rg=ref_insert_pos(ref,v.offset,v.size,rp);
            if (ag!=rg || ag) return 40; // already-free range overlaps the free list and must be rejected
        }
        if (!same(a,ref)) return 50;
    }
    std::printf("PASS: Type-F allocator golden (1,000,000 randomized operations)\n");
    return 0;
}
'''.replace("__BODIES__", bodies)

    with tempfile.TemporaryDirectory(prefix="xemu-allocator-golden-") as td:
        td = pathlib.Path(td)
        cpp = td / "allocator-golden.cpp"
        exe = td / "allocator-golden"
        cpp.write_text(generated, encoding="utf-8")
        subprocess.run([args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(cpp), "-o", str(exe)], check=True, cwd=root)
        subprocess.run([str(exe)], check=True, cwd=root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
