#include<stdio.h>
#include<assert.h>

int main()
{
    int rt, ach, acl, dsp;
    int result;

    ach = 0x05;
    acl = 0xB4CB;
    dsp = 0x07;
    result = 0x000C;

    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %2, $ac1\n\t"
         "mtlo %3, $ac1\n\t"
         "extp %0, $ac1, 0x03\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(ach), "r"(acl)
        );
    dsp = (dsp >> 14) & 0x01;
    assert(dsp == 0);
    assert(result == rt);

    ach = 0x05;
    acl = 0xB4CB;
    dsp = 0x01;

    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %2, $ac1\n\t"
         "mtlo %3, $ac1\n\t"
         "extp %0, $ac1, 0x03\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(ach), "r"(acl)
        );
    dsp = (dsp >> 14) & 0x01;
    assert(dsp == 1);

    ach = 0;
    acl = 0x80000001;
    dsp = 0x1F;
    result = 0x80000001;

    __asm
        ("wrdsp %1\n\t"
         "mthi %2, $ac2\n\t"
         "mtlo %3, $ac2\n\t"
         "extp %0, $ac2, 0x1F\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(ach), "r"(acl)
        );
    dsp = (dsp >> 14) & 0x01;
    assert(dsp == 0);
    assert(result == rt);

    return 0;
}
