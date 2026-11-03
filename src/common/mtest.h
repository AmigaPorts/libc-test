#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#undef RN
#undef RZ
#undef RD
#undef RU
#ifdef FE_TONEAREST
#define RN FE_TONEAREST
#else
#define RN 0
#endif
#ifdef FE_TOWARDZERO
#define RZ FE_TOWARDZERO
#else
#define RZ -1
#endif
#ifdef FE_DOWNWARD
#define RD FE_DOWNWARD
#else
#define RD -1
#endif
#ifdef FE_UPWARD
#define RU FE_UPWARD
#else
#define RU -1
#endif

/* A runtime that reports math errors through errno alone, without the
   exception flags, gets its errno checked against the flags the tests
   expect.  Its fenv.h may not define the flags at all, so they get bits of
   their own, which only say what the test expects.  */
#if defined(math_errhandling) && (math_errhandling & MATH_ERRNO) \
    && !(math_errhandling & MATH_ERREXCEPT)
#define MTEST_ERRNO_ONLY
#endif

#undef INEXACT
#undef INVALID
#undef DIVBYZERO
#undef UNDERFLOW
#undef OVERFLOW
#ifdef FE_INEXACT
#define INEXACT FE_INEXACT
#elif defined MTEST_ERRNO_ONLY
#define INEXACT (1<<24)
#else
#define INEXACT 0
#endif
#ifdef FE_INVALID
#define INVALID FE_INVALID
#elif defined MTEST_ERRNO_ONLY
#define INVALID (1<<25)
#else
#define INVALID 0
#endif
#ifdef FE_DIVBYZERO
#define DIVBYZERO FE_DIVBYZERO
#elif defined MTEST_ERRNO_ONLY
#define DIVBYZERO (1<<26)
#else
#define DIVBYZERO 0
#endif
#ifdef FE_UNDERFLOW
#define UNDERFLOW FE_UNDERFLOW
#elif defined MTEST_ERRNO_ONLY
#define UNDERFLOW (1<<27)
#else
#define UNDERFLOW 0
#endif
#ifdef FE_OVERFLOW
#define OVERFLOW FE_OVERFLOW
#elif defined MTEST_ERRNO_ONLY
#define OVERFLOW (1<<28)
#else
#define OVERFLOW 0
#endif

#undef inf
#undef nan
#define inf INFINITY
#define nan NAN

#define T(...) {__FILE__, __LINE__, __VA_ARGS__},

#define POS char *file; int line;
struct d_d {POS int r; double x; double y; float dy; int e; };
struct f_f {POS int r; float x; float y; float dy; int e; };
struct l_l {POS int r; long double x; long double y; float dy; int e; };
struct ff_f {POS int r; float x; float x2; float y; float dy; int e; };
struct dd_d {POS int r; double x; double x2; double y; float dy; int e; };
struct ll_l {POS int r; long double x; long double x2; long double y; float dy; int e; };
struct d_di {POS int r; double x; double y; float dy; long long i; int e; };
struct f_fi {POS int r; float x; float y; float dy; long long i; int e; };
struct l_li {POS int r; long double x; long double y; float dy; long long i; int e; };
struct di_d {POS int r; double x; long long i; double y; float dy; int e; };
struct fi_f {POS int r; float x; long long i; float y; float dy; int e; };
struct li_l {POS int r; long double x; long long i; long double y; float dy; int e; };
struct d_i {POS int r; double x; long long i; int e; };
struct f_i {POS int r; float x; long long i; int e; };
struct l_i {POS int r; long double x; long long i; int e; };
struct d_dd {POS int r; double x; double y; float dy; double y2; float dy2; int e; };
struct f_ff {POS int r; float x; float y; float dy; float y2; float dy2; int e; };
struct l_ll {POS int r; long double x; long double y; float dy; long double y2; float dy2; int e; };
struct ff_fi {POS int r; float x; float x2; float y; float dy; long long i; int e; };
struct dd_di {POS int r; double x; double x2; double y; float dy; long long i; int e; };
struct ll_li {POS int r; long double x; long double x2; long double y; float dy; long long i; int e; };
struct fff_f {POS int r; float x; float x2; float x3; float y; float dy; int e; };
struct ddd_d {POS int r; double x; double x2; double x3; double y; float dy; int e; };
struct lll_l {POS int r; long double x; long double x2; long double x3; long double y; float dy; int e; };
#undef POS

char *estr(int);
char *rstr(int);

float ulperr(double got, double want, float dwant);
float ulperrf(float got, float want, float dwant);
float ulperrl(long double got, long double want, float dwant);

/* errno as the function under test left it: the tests call feclearexcept()
   right before it, which clears errno too, and fetestexcept() right after,
   whose first call keeps errno.  */
static int mtest_errno, mtest_errno_state;

static int mtest_feclearexcept(int e)
{
	errno = 0;
	mtest_errno_state = 1;
	return feclearexcept(e);
}

static int mtest_fetestexcept(int e)
{
	if (mtest_errno_state == 1) {
		mtest_errno = errno;
		mtest_errno_state = 2;
	}
	return fetestexcept(e);
}

#define feclearexcept mtest_feclearexcept
#define fetestexcept mtest_fetestexcept

/* Whether file, the test's source, is the test of one of names: of
   "exp.c" for "exp", or of its float and long double variants.  */
static int mtest_func(const char *file, const char *const *names)
{
	const char *b = strrchr(file, '/');
	size_t n;

	b = b ? b + 1 : file;
	n = strcspn(b, ".");
	for (; *names; names++) {
		size_t m = strlen(*names);
		if (!strncmp(b, *names, m)
		    && (n == m || (n == m + 1 && (b[m] == 'f' || b[m] == 'l'))))
			return 1;
	}
	return 0;
}

/* With MATH_ERRNO, C11 7.12.1 wants errno to be EDOM after a domain error
   and ERANGE after a pole error or an overflow; after an underflow ERANGE
   is up to the implementation.  Here a domain error is a result that raises
   INVALID, a pole error DIVBYZERO, and the range errors OVERFLOW and
   UNDERFLOW, so:
   - with none of them, errno stays 0: the math functions document their use
     of errno, so they may not set it otherwise (C11 7.5p3);
   - with INVALID or DIVBYZERO, it is 0, EDOM or ERANGE, as several of these
     are "a domain error or a pole error may occur";
   - with OVERFLOW or UNDERFLOW alone, it is 0 or ERANGE;
   - it must be set where C says the error "occurs", not that it "may":
     EDOM for the INVALID results of the functions in domain_errors, and
     ERANGE for the OVERFLOW results of those in overflows.
   Outside round-to-nearest, as checkexcept() does for the flag, an
   underflow may or may not be there.  */
static int mtest_checkerrno(const char *file, int want, int r)
{
	static const char *const domain_errors[] = {
		"acos", "asin", "acosh", "atanh", "sqrt",
		"log", "log10", "log2", "log1p", 0
	};
	static const char *const overflows[] = {
		"exp", "exp2", "expm1", "cosh", "sinh", "lgamma", "tgamma", 0
	};
	/* not in C, so not covered by math_errhandling */
	static const char *const not_c[] = {
		"j0", "j1", "jn", "y0", "y1", "yn", "drem", "scalb",
		"lgamma_r", "exp10", "pow10", "significand", 0
	};
	/* "If y is zero, whether a domain error occurs ... is implementation
	   defined", also when x is a NaN */
	static const char *const y_zero[] = { "fmod", "remainder", "remquo", 0 };
	int err = want & (INVALID|DIVBYZERO|OVERFLOW|UNDERFLOW);
	int e = mtest_errno;
	int ok;

#ifdef math_errhandling
	if (!(math_errhandling & MATH_ERRNO))
		return 1;
#else
	return 1;	/* nothing promised */
#endif
	if (mtest_errno_state != 2)
		return 1;	/* the test doesn't bracket the call */
	if (mtest_func(file, not_c))
		return 1;
	if (!err)
		ok = e == 0 || (r != RN && e == ERANGE)
		     || (e == EDOM && mtest_func(file, y_zero));
	else if (want & (INVALID|DIVBYZERO))
		ok = e == 0 || e == EDOM || e == ERANGE;
	else
		ok = e == 0 || e == ERANGE;
	if (ok && (want & INVALID) && mtest_func(file, domain_errors))
		ok = e == EDOM;
	if (ok && (want & OVERFLOW) && mtest_func(file, overflows))
		ok = e == ERANGE;
	if (!ok)
		printf("%s: errno %s after a call that %s\n", file,
		       e == EDOM ? "EDOM" : e == ERANGE ? "ERANGE" : e ? "set" : "0",
		       err ? "should report an error" : "should not set errno");
	return ok;
}

static int checkexceptflags(int got, int want, int r)
{
#ifdef math_errhandling
	/* without MATH_ERREXCEPT, C doesn't require the math functions to
	   raise any flags (C11 7.12.1), so there is nothing to compare */
	if (!(math_errhandling & MATH_ERREXCEPT))
		return 1;
#endif
	if (r == RN)
#if defined CHECK_INEXACT
		return got == want;
#elif defined CHECK_INEXACT_OMISSION
		return got == want || got == (want|INEXACT);
#else
		return (got|INEXACT) == (want|INEXACT);
#endif
	return (got|INEXACT|UNDERFLOW) == (want|INEXACT|UNDERFLOW);
}

static int checkexceptallflags(int got, int want, int r)
{
#if defined MTEST_ERRNO_ONLY && !defined FE_INEXACT
	/* the flags are only what the test expects: none are raised */
	return 1;
#endif
	return got == want;
}

/* Both check the flags and errno, each where math_errhandling promises it.
   Macros, for the test's __FILE__.  */
#define checkexcept(got, want, r) \
	(checkexceptflags(got, want, r) & mtest_checkerrno(__FILE__, want, r))
#define checkexceptall(got, want, r) \
	(checkexceptallflags(got, want, r) & mtest_checkerrno(__FILE__, want, r))

static int checkulp(float d, int r)
{
	// TODO: we only care about >=1.5 ulp errors for now, should be 1.0
	if (r == RN)
		return fabsf(d) < 1.5;
	// accept larger error in non-nearest rounding mode
	return fabsf(d) < 3.0;
}

static int checkcr(long double y, long double ywant, int r)
{
	if (isnan(ywant))
		return isnan(y);
	return y == ywant && signbit(y) == signbit(ywant);
}

