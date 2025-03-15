uniform uint* data_ptr; // creates a ptr chain access

struct Data{
   int x;
}
uniform Data* data_struct;

[numthreads(1,1,1)]
void computeMain()
{
    data_ptr[0] = 1;
    data_struct->x = 1;
}
