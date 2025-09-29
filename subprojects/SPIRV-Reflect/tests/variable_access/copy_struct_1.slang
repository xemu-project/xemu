struct Bar {
    uint x;
    uint y;
    uint z[2];
};

struct FooBuffer1 {
    float4 a;
    Bar b;
    uint d;
};

struct FooBuffer2 {
    float4 d;
    Bar e;
    uint f;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<FooBuffer1> foo1;

[[vk::binding(0, 1)]]
RWStructuredBuffer<FooBuffer2> foo2;

[shader("compute")]
void main() {
    foo1[0].b = foo2[0].e;
}