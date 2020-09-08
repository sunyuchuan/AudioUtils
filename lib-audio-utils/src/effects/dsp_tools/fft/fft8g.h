#ifndef FFT8G_H_
#define FFT8G_H_

#define FFT8G_FLOAT

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FFT8G_FLOAT
void ae_cdft_f(int, int, float *, int *, float *);
void ae_rdft_f(int, int, float *, int *, float *);
void ae_ddct_f(int, int, float *, int *, float *);
void ae_ddst_f(int, int, float *, int *, float *);
void ae_dfct_f(int, float *, float *, int *, float *);
void ae_dfst_f(int, float *, float *, int *, float *);
#else
void ae_cdft(int, int, double *, int *, double *);
void ae_rdft(int, int, double *, int *, double *);
void ae_ddct(int, int, double *, int *, double *);
void ae_ddst(int, int, double *, int *, double *);
void ae_dfct(int, double *, double *, int *, double *);
void ae_dfst(int, double *, double *, int *, double *);
#endif

#ifdef __cplusplus
}
#endif
#endif  // FFT8G_H_
