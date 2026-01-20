struct DataUsed{
    int unused_var_2;
    int used_var_2;
};

struct DataUnused{
    int unused_var_3;
};

struct Data{
    DataUnused unused_struct;
    DataUsed used_struct;
}
uniform Data* data_struct;

[numthreads(1,1,1)]
void computeMain()
{
    data_struct->used_struct.used_var_2 = 1;
}
