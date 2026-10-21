#version 450
#extension GL_EXT_debug_printf : enable

void main()
{
    debugPrintfEXT("double quote: \"");
    debugPrintfEXT("backslash: \\");
    debugPrintfEXT("bell: \a");
    debugPrintfEXT("backspace: \b");
    debugPrintfEXT("form feed: \f");
    debugPrintfEXT("newline: \n");
    debugPrintfEXT("carriage return: \r");
    debugPrintfEXT("tab: \t");
    debugPrintfEXT("vertical tab: \v");
}