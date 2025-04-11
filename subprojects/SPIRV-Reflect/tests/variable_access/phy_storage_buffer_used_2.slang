uniform uint* data_ptr; // creates a ptr chain access
uniform uint* data_ptr_usused;

struct Data{
    int unused_var;
    int used_var;
}
uniform Data* data_struct;

[numthreads(1,1,1)]
void computeMain()
{
    data_ptr[0] = 1;
    data_struct->used_var = 1;
}
