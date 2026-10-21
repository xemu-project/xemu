// Test: guard against elseSeen[elsetracker] OOB write when elsetracker < 0.
//
// A malformed #if expression causes eval() to return err=true; CPPif increments
// ifdepth/elsetracker but skips the branch scan.  The subsequent nested
// #if 0 / #endif block enters CPPelse with depth > 0, driving ifdepth negative
// via the unguarded --ifdepth at Pp.cpp:328.  Once ifdepth < 0 the
// readCPPline guard (if (ifdepth == 0)) is permanently bypassed, and every
// further #endif decrements elsetracker without bound.
//
// Without the fix, elseSeen[elsetracker] writes 0x00 backward through TPpContext
// into lastLineTokenLocs, crashing in scanToken (PpContext.h:384).
// With the fix, both write sites check elsetracker >= 0, and the unguarded
// --ifdepth is protected by if (ifdepth > 0).

#version 450

// Step 1: bad expression desynchronises ifdepth and elsetracker.
#if %%%
#endif

// Step 2: nested block enters CPPelse depth>0 path, ifdepth goes negative.
#if 0
  #if 1
    #if 1
    #endif
  #endif
#endif

// Step 3: excess #endifs hit readCPPline with ifdepth < 0; without the
// elsetracker >= 0 guard they would write OOB into the struct.
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

void main() {}
