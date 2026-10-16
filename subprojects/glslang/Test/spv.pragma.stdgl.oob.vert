#version 450
// Regression test: #pragma STDGL with fewer than 4 tokens must not OOB-read
// tokens[1]/tokens[3] in handlePragma() — fixed by checking tokens.size() >= 4
// before accessing them (ParseHelper.cpp).
#pragma STDGL
#pragma STDGL invariant
#pragma STDGL invariant(
#pragma STDGL something (all)
#pragma STDGL invariant []
#pragma STDGL invariant [all]
#pragma STDGL invariant ( some )

void main() {}
