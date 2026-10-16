#if (1 << 100) != 0
#error shifting left by more than the width should yield 0, something went wrong.
#endif
#if (1 >> 100) != 0
#error shifting right by more than the width should yield 0, something went wrong.
#endif
#if (-1 >> 100) != -1
#error shifting a negative right by more than the width should sign fill, something went wrong.
#endif
#if (1 << -1) != 0
#error shifting left by a negative amount should yield 0, something went wrong.
#endif
#if (1 >> -1) != 0
#error shifting right by a negative amount should yield 0, something went wrong.
#endif
#if (-1 << 1) != -2
#error shifting a negative left should wrap, something went wrong.
#endif
#if (2147483647 + 1) != -2147483648
#error addition should wrap, something went wrong.
#endif
#if (-2147483648 - 1) != 2147483647
#error subtraction should wrap, something went wrong.
#endif
#if (2147483647 * 2) != -2
#error multiplication should wrap, something went wrong.
#endif
