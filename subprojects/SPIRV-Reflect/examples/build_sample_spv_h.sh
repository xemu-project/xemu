#!/bin/sh

echo -e "#ifndef SAMPLE_SPV_H" >  sample_spv.h
echo -e "#define SAMPLE_SPV_H" >> sample_spv.h

echo -e "" >> sample_spv.h
echo -e "/* Source from sample.hlsl" >> sample_spv.h
echo -e "" >> sample_spv.h
cat sample.hlsl >> sample_spv.h
echo -e "\n" >> sample_spv.h
echo -e "*/" >> sample_spv.h

echo -e "" >> sample_spv.h

echo -e "// Imported from file 'sample.spv'" >> sample_spv.h
echo -e "const uint32_t k_sample_spv[] = {" >> sample_spv.h
glslc.exe -fshader-stage=frag -fentry-point=main -mfmt=num -o - sample.hlsl >> sample_spv.h
echo -e "};" >> sample_spv.h
echo -e "" >> sample_spv.h

echo -e "/* SPIRV Disassembly" >> sample_spv.h
echo -e "" >> sample_spv.h
spirv-dis --raw-id sample.spv >> sample_spv.h 
echo -e "" >> sample_spv.h
echo -e "*/" >> sample_spv.h

echo -e "" >> sample_spv.h
echo -e "#endif // SAMPLE_SPV_H" >> sample_spv.h

dos2unix sample_spv.h

rm -f tmp_sample_spv_h