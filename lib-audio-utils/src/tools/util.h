#ifndef AUDIO_EFFECT_UTIL_H
#define AUDIO_EFFECT_UTIL_H
#include <stddef.h>
#include <stdio.h>

#ifdef __linux__
#define HAVE_STRCASECMP 1
#endif

extern int ae_strcasecmp(const char *s1, const char *st);
extern int ae_strncasecmp(char const *s1, char const *s2, size_t n);
extern int ae_open_file(FILE **fp, const char *file_name, const int is_write);
extern char *ae_read_file_to_string(const char *filename);

#ifndef HAVE_STRCASECMP
#define strcasecmp(s1, s2) ae_strcasecmp((s1), (s2))
#define strncasecmp(s1, s2, n) ae_strncasecmp((s1), (s2), (n))
#endif

/*--------------------------- Language extensions ----------------------------*/

/* Compile-time ("static") assertion */
/*   e.g. assert_static(sizeof(int) >= 4, int_type_too_small)    */
#define assert_static(e, f) enum { assert_static__##f = 1 / (e) }
#define array_length(a) (sizeof(a) / sizeof(a[0]))
#define field_offset(type, field) ((size_t) & (((type *)0)->field))
#define unless(x) if (!(x))

/*------------------------------- Maths stuff --------------------------------*/

#include <math.h>

#ifdef min
#undef min
#endif
#define min(a, b) ((a) <= (b) ? (a) : (b))

#ifdef max
#undef max
#endif
#define max(a, b) ((a) >= (b) ? (a) : (b))

#define range_limit(x, lower, upper) (min(max(x, lower), upper))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923 /* pi/2 */
#endif
#ifndef M_LN10
#define M_LN10 2.30258509299404568402 /* natural log of 10 */
#endif
#ifndef M_SQRT2
#define M_SQRT2 sqrt(2.)
#endif

#define sqr(a) ((a) * (a))
#define sign(x) ((x) < 0 ? -1 : 1)

/* Numerical Recipes in C, p. 284 */
#define ranqd1(x) ((x) = 1664525L * (x) + 1013904223L)    /* int32_t x */
#define dranqd1(x) (ranqd1(x) * (1. / (65536. * 32768.))) /* [-1,1) */

#define dB_to_linear(x) exp((x)*M_LN10 * 0.05)
#define linear_to_dB(x) (log10(x) * 20)

#endif  // AUDIO_EFFECT_UTIL_H
