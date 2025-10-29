RWStructuredBuffer<int> result;

struct Data2{
   int x;
   int* y;
};

struct Data{
   int a;
   Data2* b;
   int c;
};
[[vk::push_constant]] Data pc;

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    result[0] = pc.b[2].y[2] + pc.c;
}