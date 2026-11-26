#include <fenv.h>
#include <pizlonated_math.h>

int feclearexcept(int mask)
{
	zmath_feclearexcept(mask);
	return 0;
}

int feraiseexcept(int mask)
{
	return zmath_feraiseexcept(mask);
}

int fetestexcept(int mask)
{
	return zmath_fetestexcept(mask);
}

int fegetround(void)
{
	return zmath_fegetround();
}

int __fesetround(int r)
{
	return zmath_fesetround(r);
}

int fegetenv(fenv_t *envp)
{
	return zmath_fegetenv(envp);
}

int fesetenv(const fenv_t *envp)
{
	return zmath_fesetenv(envp);
}
