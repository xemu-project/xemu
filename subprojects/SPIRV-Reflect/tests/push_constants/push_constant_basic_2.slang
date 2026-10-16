RWStructuredBuffer<uint> result;

struct Bar{
   uint t;
   uint s;
};

struct Data{
   uint a;
   uint b[4];
   Bar bar;
   uint* node;
};
[[vk::push_constant]] Data pc;

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    result[0] += pc.a;
    result[0] += pc.b[1];
    result[0] += pc.bar.s;
    result[0] += pc.node[3];
}