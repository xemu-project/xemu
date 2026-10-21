#version 460
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_structured_descriptor_heap : require

layout(descriptor_heap) readonly buffer ReadSSBO {
    uint value;
} reads[];

layout(buffer_type) writeonly buffer WriteSSBO {
    uint value;
};

layout(heap_offset = 256) resourceheap WriteHeap {
    WriteSSBO writes[];
} writeHeap;

void main()
{
    writeHeap.writes[1].value = reads[0].value;
}
