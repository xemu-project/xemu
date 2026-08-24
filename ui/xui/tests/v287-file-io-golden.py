#!/usr/bin/env python3
# v2.87 current regression ownership.
from __future__ import annotations
import argparse,pathlib

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--root',default='.')
    root=pathlib.Path(ap.parse_args().root).resolve(); cc=(root/'ui/xui/debug-tools/current-game.cc').read_text()
    assert 'sha256_disc_file' not in cc
    assert 'sha256_bytes(xbe_file, hash_error)' in cc
    assert 'm_disc_xbe_file = std::move(xbe_file);' in cc
    assert 'read_local_binary_file(path, 512ull * 1024ull * 1024ull' in cc
    assert 'read_local_text_file(path, 64ull * 1024ull * 1024ull' in cc
    pdb=cc[cc.index('bool CurrentGameManager::LoadPdbFile'):cc.index('bool CurrentGameManager::SaveLabelPackFile')]
    assert 'g_file_get_contents' not in pdb
    assert 'std::vector<uint8_t> pdb_file((const uint8_t *)contents' not in pdb
    mp=cc[cc.index('bool CurrentGameManager::LoadMapFile'):cc.index('bool CurrentGameManager::LoadPdbFile')]
    assert 'std::string(contents, length)' not in mp
    print('PASS: v1.83 one-read XBE + direct MAP/PDB destination-buffer I/O invariants')
if __name__=='__main__':main()
