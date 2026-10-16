#version 430 core
#extension GL_EXT_split_barrier : enable

void main()
{
    controlBarrierArrive(); // ERROR
    controlBarrierWait();   // ERROR
}
