#version 450
#extension GL_EXT_debug_printf : enable

// a macro that expands to a string literal token is still a string literal
#define FMT "macro expanded: %d"

void main()
{
    debugPrintfEXT("ASDF \\ \? \x5C %d %d %d", 1, 2, 3);

    // ABA{backspace}Z
    debugPrintfEXT("\x41\x000042\x41\x8Z");

    // B#${bell, aka \a}B1Z
    debugPrintfEXT("\102\043\44\7\1021Z");

    debugPrintfEXT(FMT, 4);
}
