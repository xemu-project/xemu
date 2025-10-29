#include<stdio.h>
#include<assert.h>

int main()
{
    int rt, rs, ach, acl, dsp, pos, efi;
    int result;

    ach = 0x05;
    acl = 0xB4CB;
    dsp = 0x07;
    rs  = 0x03;
    result = 0x000C;

    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %2, $ac1\n\t"
         "mtlo %3, $ac1\n\t"
         "extpdpv %0, $ac1, %4\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(ach), "r"(acl), "r"(rs)
        );
    pos =  dsp & 0x3F;
    efi = (dsp >> 14) & 0x01;
    assert(pos == 3);
    assert(efi == 0);
    assert(result == rt);

    ach = 0x05;
    acl = 0xB4CB;
    dsp = 0x01;

    __asm
        ("wrdsp %1, 0x01\n\t"
         "mthi %2, $ac1\n\t"
         "mtlo %3, $ac1\n\t"
         "extpdpv %0, $ac1, %4\n\t"
         "rddsp %1\n\t"
         : "=r"(rt), "+r"(dsp)
         : "r"(ach), "r"(acl), "r"(rs)
        );
    efi = (dsp >> 14) & 0x01;
    assert(efi == 1);

    return 0;
}
