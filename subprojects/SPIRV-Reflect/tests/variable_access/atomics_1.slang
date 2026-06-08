struct SSBOData {
    Atomic<int> a;
    Atomic<int> b; // UNUSED
    Atomic<int> c;
    Atomic<int> d; // UNUSED
    Atomic<int> e; // UNUSED
    Atomic<int> f;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<SSBOData> ssbo;

[shader("compute")]
void main() {
    uint x = ssbo[0].a.load();
    ssbo[0].c.exchange(0);
    ssbo[0].f.store(0);
}