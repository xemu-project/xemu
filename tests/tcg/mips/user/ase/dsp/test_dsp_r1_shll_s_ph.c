#include<stdio.h>
#include<assert.h>

int main()
{
    int rd, rt, dsp;
    int result, resultdsp;

    rt        = 0x12345678;
    result    = 0x7FFF7FFF;
    resultdsp = 0x01;

    __asm
        ("shll_s.ph %0, %2, 0x0B\n\t"
         "rddsp %1\n\t"
         : "=r"(rd), "=r"(dsp)
         : "r"(rt)
        );
    dsp = (dsp >> 22) & 0x01;
    assert(dsp == resultdsp);
    assert(rd  == result);

    return 0;
}
