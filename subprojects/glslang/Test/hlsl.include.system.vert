// For -Iinc1 -Iinc1/path1 -Iinc1/path2
// Should include ./inc1/bar, ./inc1/foo, and ./parent
#include <bar.h>
// Should include ./inc1/path1/notHere.h
#include <notHere.h>

float4 main() : SV_Position
{
    return i2 + i3 + i4 + p1 + p2 + p3;
}
