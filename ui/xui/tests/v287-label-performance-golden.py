#!/usr/bin/env python3
# v2.87 current regression ownership.
from __future__ import annotations
import argparse, pathlib
from v287_source_test_utils import read_memory_tools_implementation

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--root',default='.')
    root=pathlib.Path(ap.parse_args().root).resolve(); d=root/'ui/xui/debug-tools'
    cc=read_memory_tools_implementation(d); hh=(d/'memory-tools.hh').read_text(); cg=(d/'current-game.cc').read_text(); cgh=(d/'current-game.hh').read_text()
    for token in ('m_visible_label_cache','m_visible_label_generation','m_visible_label_search','filter_changed'):
        assert token in cc or token in hh, token
    assert 'current_game_manager.LabelGeneration()' in cc
    assert 'uint64_t LabelGeneration() const' in cgh
    reload=cg[cg.index('bool CurrentGameManager::ReloadLabelPacks()'):cg.index('bool CurrentGameManager::RefreshXdkLabels',cg.index('bool CurrentGameManager::ReloadLabelPacks()'))]
    assert reload.count('XemuXbeLabels::SortAndUnique(m_labels)') <= 2
    assert 'XemuXbeLabels::Append(m_labels, m_xdk_labels)' in reload
    assert 'XemuXbeLabels::Append(m_labels, m_map_labels)' in reload
    assert 'XemuXbeLabels::Append(m_labels, m_pdb_labels)' in reload
    assert 'false, true' in reload
    print('PASS: v1.82 label filter-cache + batch-merge invariants')
if __name__=='__main__': main()
