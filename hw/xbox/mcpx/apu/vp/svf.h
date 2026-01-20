/*
 * Adapted from SWH LADSPA Plugins package, modified for xemu
 *
 * Source:  https://github.com/swh/ladspa/blob/master/svf_1214.xml
 * Author:  Steve Harris, andy@vellocet
 * License: GPLv2
 *
 */

#ifndef SVF_H
#define SVF_H

#include <math.h>

#define flush_to_zero(x) x

// Constants to match filter types
#define F_LP 1
#define F_HP 2
#define F_BP 3
#define F_BR 4
#define F_AP 5

// Number of filter oversamples
#define F_R 1

/* Structure to hold parameters for SV filter */

typedef struct {
    float f;     // 2.0*sin(PI*fs/(fc*r));
    float q;     // 2.0*cos(pow(q, 0.1)*PI*0.5);
    float qnrm;  // sqrt(m/2.0f+0.01f);
    float h;     // high pass output
    float b;     // band pass output
    float l;     // low pass output
    float p;     // peaking output (allpass with resonance)
    float n;     // notch output
    float *op;   // pointer to output value
} sv_filter;

/* Store data in SVF struct, takes the sampling frequency, cutoff frequency
   and Q, and fills in the structure passed */
/*
static inline void setup_svf(sv_filter *sv, float fs, float fc, float q, int t) {
    sv->f = 2.0f * sin(M_PI * fc / (float)(fs * F_R));
    sv->q = 2.0f * cos(pow(q, 0.1f) * M_PI * 0.5f);
*/
static inline void setup_svf(sv_filter *sv, float fc, float q, int t) {
    sv->f = fc;
    sv->q = q;
    sv->qnrm = sqrt(sv->q/2.0+0.01);
    switch(t) {
    case F_LP:
        sv->op = &(sv->l);
        break;
    case F_HP:
        sv->op = &(sv->h);
        break;
    case F_BP:
        sv->op = &(sv->b);
        break;
    case F_BR:
        sv->op = &(sv->n);
        break;
    default:
        sv->op = &(sv->p);
    }
}

/* Run one sample through the SV filter. Filter is by andy@vellocet */
static inline float run_svf(sv_filter *sv, float in) {
    float out;
    int i;
    in = sv->qnrm * in ;
    for (i=0; i < F_R; i++) {
        // very slight waveshape for extra stability
        sv->b = flush_to_zero(sv->b - sv->b * sv->b * sv->b * 0.001f);
        // regular state variable code here
        // the notch and peaking outputs are optional
        sv->h = flush_to_zero(in - sv->l - sv->q * sv->b);
        sv->b = sv->b + sv->f * sv->h;
        sv->l = flush_to_zero(sv->l + sv->f * sv->b);
        sv->n = sv->l + sv->h;
        sv->p = sv->l - sv->h;
        out = *(sv->op);
        in = out;
    }
    return out;
}

#endif
