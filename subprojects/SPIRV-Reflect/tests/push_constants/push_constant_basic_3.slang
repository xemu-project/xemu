RWStructuredBuffer<uint> result;

struct Foo{
    uint s;
    uint t;
};

struct Bar{
    uint x_unused;
    uint y;
};

struct Data{
   uint a_unused;
   uint b[4];
   uint c;
   uint d_unused[4];
   Foo* foo_unused;
   Bar* bar;
};
[[vk::push_constant]] Data pc;

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    result[0] += pc.c;
    result[0] += pc.b[1];
    result[0] += pc.bar->y;
}