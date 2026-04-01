// math.h - stub for wasm32
#pragma once

#define HUGE_VAL  __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define INFINITY  __builtin_inff()
#define NAN       __builtin_nanf("")

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

extern double acos(double x);
extern double asin(double x);
extern double atan(double x);
extern double atan2(double y, double x);
extern double cos(double x);
extern double sin(double x);
extern double tan(double x);
extern double cosh(double x);
extern double sinh(double x);
extern double tanh(double x);
extern double acosh(double x);
extern double asinh(double x);
extern double atanh(double x);
extern double exp(double x);
extern double exp2(double x);
extern double expm1(double x);
extern double log(double x);
extern double log10(double x);
extern double log2(double x);
extern double log1p(double x);
extern double pow(double x, double y);
extern double sqrt(double x);
extern double cbrt(double x);
extern double hypot(double x, double y);
extern double fabs(double x);
extern double ceil(double x);
extern double floor(double x);
extern double trunc(double x);
extern double round(double x);
extern double fmod(double x, double y);
extern double remainder(double x, double y);
extern double copysign(double x, double y);
extern double nan(const char *tagp);
extern double frexp(double x, int *exp);
extern double ldexp(double x, int exp);
extern double modf(double x, double *iptr);
extern double scalbn(double x, int n);
extern int ilogb(double x);
extern double logb(double x);
extern double nextafter(double x, double y);
extern double fdim(double x, double y);
extern double fmax(double x, double y);
extern double fmin(double x, double y);
extern double fma(double x, double y, double z);
extern int isnan(double x);
extern int isinf(double x);
extern int isfinite(double x);
extern int fpclassify(double x);
extern int signbit(double x);
extern long lround(double x);
extern long long llround(double x);
extern long lrint(double x);
extern long long llrint(double x);
extern double rint(double x);
extern double nearbyint(double x);

// Float versions
extern float acosf(float x);
extern float asinf(float x);
extern float atanf(float x);
extern float atan2f(float y, float x);
extern float cosf(float x);
extern float sinf(float x);
extern float tanf(float x);
extern float coshf(float x);
extern float sinhf(float x);
extern float tanhf(float x);
extern float expf(float x);
extern float exp2f(float x);
extern float logf(float x);
extern float log10f(float x);
extern float log2f(float x);
extern float powf(float x, float y);
extern float sqrtf(float x);
extern float cbrtf(float x);
extern float fabsf(float x);
extern float ceilf(float x);
extern float floorf(float x);
extern float truncf(float x);
extern float roundf(float x);
extern float fmodf(float x, float y);
extern float copysignf(float x, float y);
extern long lroundf(float x);
extern long long llroundf(float x);

// Long double versions (same as double on wasm32)
extern long double acosl(long double x);
extern long double asinl(long double x);
extern long double atanl(long double x);
extern long double atan2l(long double y, long double x);
extern long double cosl(long double x);
extern long double sinl(long double x);
extern long double tanl(long double x);
extern long double expl(long double x);
extern long double logl(long double x);
extern long double log10l(long double x);
extern long double powl(long double x, long double y);
extern long double sqrtl(long double x);
extern long double fabsl(long double x);
extern long double ceill(long double x);
extern long double floorl(long double x);
extern long double truncl(long double x);
extern long double roundl(long double x);
extern long double fmodl(long double x, long double y);
