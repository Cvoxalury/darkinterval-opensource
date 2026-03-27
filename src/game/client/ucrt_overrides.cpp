//========================================================================//
//
// Purpose: UCRT overrides
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"

#ifdef DARKINTERVAL

#if _MSC_VER >= 1900

#include <limits>

// Can't include cmath directly as it has hypot already defined.
_Check_return_ _CRT_JIT_INTRINSIC double __cdecl sqrt(_In_ double _X);

// libucrt(d).lib defines _hypot as a part of C++11.
// particles.lib(particle_sort.obj) does the same.
//
// What we do for modern compilers is to explicitly define _hypot here, as linker chooses impl from obj first.

// Correct implementation is NOT naive one as have a lot of special requirements:
// 
// If no errors occur, the hypotenuse of a right - angled triangle, sqrt(x^2 + y^2), is returned.
// If a range error due to overflow occurs, +HUGE_VAL, +HUGE_VALF, or +HUGE_VALL is returned.
// If a range error due to underflow occurs, the correct result(after rounding) is returned.
//
// If the implementation supports IEEE floating - point arithmetic(IEC 60559),
// std::hypot(x, y), std::hypot(y, x), and std::hypot(x, -y) are equivalent.
// if one of the arguments is +-0, std::hypot(x, y) is equivalent to std::fabs called with the non-zero argument.
// if one of the arguments is +-inf, std::hypot(x, y) returns +inf even if the other argument is NaN.
// otherwise, if any of the arguments is NaN, NaN is returned.
double _hypot(double x, double y)
{
	// Based on https://github.com/microsoft/STL/blob/082d80ae53925cad65e5f0b6a3ca00fd786fb96d/stl/src/special_math.cpp#L482
	static_assert(std::numeric_limits<double>::is_iec559, "Double should be IEC559 aka IEEE 754.");

	// std::hypot(x, y), std::hypot(y, x), and std::hypot(x, -y) are equivalent.
	x = std::abs(x);
	y = std::abs(y);

	// if one of the arguments is +-inf, std::hypot(x, y) returns +inf even if the other argument is NaN.
	static_assert(std::numeric_limits<double>::has_infinity, "Double should have infinity to work with.");
	constexpr float inf = std::numeric_limits<double>::infinity();
	if ( x == inf || y == inf )
	{
		return inf;
	}

	if (y > x)
	{
		std::swap(x, y);
	}

	constexpr float eps = std::numeric_limits<double>::epsilon();
	if ( x * eps >= y )
	{
		return x;
	}

	const auto dydx = y / x;

	return x * static_cast<double>( sqrt( 1 + dydx * dydx ) );
}

#endif

#endif