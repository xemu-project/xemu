StructuredBuffer<float3>   Input[16] : register(t0);
RWStructuredBuffer<float3> Output    : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
	float3 value = (float3)0;
	for (int i = 0; i < 16; ++i) {
		value += Input[tid.x][i];
	}
	Output[tid.x] = value;
}