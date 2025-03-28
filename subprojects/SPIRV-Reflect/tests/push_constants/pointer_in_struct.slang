RWStructuredBuffer<int> result;

struct Data{
   int a;
   int* b;
   int c;
};
[[vk::push_constant]] Data pc;

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    result[0] = pc.b[2] + pc.c;
}