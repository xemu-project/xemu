#version 450 core

layout(set = 0, binding = 0) buffer buf
{
    uint buf_Data[];
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    buf_Data[0] = buf_Data[32];
    buf_Data[100] = buf_Data[132];
    buf_Data[200] = buf_Data[232];
}
