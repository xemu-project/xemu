#version 460
#extension GL_KHR_memory_scope_semantics : enable

layout(set=0, binding=0) buffer SSBO {
    uint a;
    uint b; // UNUSED
    uint c;
    uint d;
    uint e; // UNUSED
    uint f;
};

void main() {
    uint x = atomicLoad(a, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsRelaxed);
    atomicStore(c, 0u, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsRelaxed);
    atomicExchange(d, f);
}