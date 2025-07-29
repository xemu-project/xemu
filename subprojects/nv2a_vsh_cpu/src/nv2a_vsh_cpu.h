#ifndef NV2A_VSH_CPU_SRC_NV2A_VSH_CPU_H_
#define NV2A_VSH_CPU_SRC_NV2A_VSH_CPU_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Nv2aVshCpuFunc)(float *out,
                               const float *inputs);

void nv2a_vsh_cpu_mov(float *out, const float *inputs);
void nv2a_vsh_cpu_arl(float *out, const float *inputs);
void nv2a_vsh_cpu_mul(float *out, const float *inputs);
void nv2a_vsh_cpu_add(float *out, const float *inputs);
void nv2a_vsh_cpu_mad(float *out, const float *inputs);
void nv2a_vsh_cpu_dp3(float *out, const float *inputs);
void nv2a_vsh_cpu_dph(float *out, const float *inputs);
void nv2a_vsh_cpu_dp4(float *out, const float *inputs);
void nv2a_vsh_cpu_dst(float *out, const float *inputs);
void nv2a_vsh_cpu_min(float *out, const float *inputs);
void nv2a_vsh_cpu_max(float *out, const float *inputs);
void nv2a_vsh_cpu_slt(float *out, const float *inputs);
void nv2a_vsh_cpu_sge(float *out, const float *inputs);
void nv2a_vsh_cpu_rcp(float *out, const float *inputs);
void nv2a_vsh_cpu_rcc(float *out, const float *inputs);
void nv2a_vsh_cpu_rsq(float *out, const float *inputs);

// WARNING: Negative inputs are not valid on hardware and are silently processed
// here.
void nv2a_vsh_cpu_exp(float *out, const float *inputs);

void nv2a_vsh_cpu_log(float *out, const float *inputs);
void nv2a_vsh_cpu_lit(float *out, const float *inputs);

#ifdef __cplusplus
};  // extern "C"
#endif

#endif  // NV2A_VSH_CPU_SRC_NV2A_VSH_CPU_H_
