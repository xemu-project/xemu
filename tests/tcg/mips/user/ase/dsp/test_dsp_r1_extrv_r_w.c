#include<stdio.h>
#include<assert.h>

int main()
{
    int rt, rs, ach, acl, dsp;
    int result;

    ach = 0x05;
    acl = 0xB4CB;
    dsp = 0x07;
    rs  = 0x03;
    result = 0xA0001699;

    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %3, $ac1\n\t"
         "mtlo %4, $ac1\n\t"
         "extrv_r.w %0, $ac1, %2\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(rs), "r"(ach), "r"(acl)
        );
    dsp = (dsp >> 23) & 0x01;
    assert(dsp == 1);
    assert(result == rt);

    /* Clear dspcontrol */
    dsp = 0;
    __asm
        ("wrdsp %0\n\t"
         :
         : "r"(dsp)
        );

    rs = 4;
    ach = 0x01;
    acl = 0xB4CB;
    result = 0x10000B4D;
    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %3, $ac1\n\t"
         "mtlo %4, $ac1\n\t"
         "extrv_r.w %0, $ac1, %2\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(rs), "r"(ach), "r"(acl)
        );
    dsp = (dsp >> 23) & 0x01;
    assert(dsp == 0);
    assert(result == rt);

    /* Clear dspcontrol */
    dsp = 0;
    __asm
        ("wrdsp %0\n\t"
         :
         : "r"(dsp)
        );

    rs = 31;
    ach = 0x3fffffff;
    acl = 0x2bcdef01;
    result = 0x7ffffffe;
    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %3, $ac1\n\t"
         "mtlo %4, $ac1\n\t"
         "extrv_r.w %0, $ac1, %2\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(rs), "r"(ach), "r"(acl)
        );
    dsp = (dsp >> 23) & 0x01;
    assert(dsp == 0);
    assert(result == rt);

    return 0;
}
