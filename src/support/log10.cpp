// Derived from FDLIBM 5.3 (https://www.netlib.org/fdlibm/): e_log10.c, whose method glibc's log10
// also follows. Licence: LICENSES/fdlibm.txt. Changes: licensing/changes/fdlibm.md
/* @(#)e_log10.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_log10(x)
 * Return the base 10 logarithm of x
 * 
 * Method :
 *	Let log10_2hi = leading 40 bits of log10(2) and
 *	    log10_2lo = log10(2) - log10_2hi,
 *	    ivln10   = 1/log(10) rounded.
 *	Then
 *		n = ilogb(x), 
 *		if(n<0)  n = n+1;
 *		x = scalbn(x,-n);
 *		log10(x) := n*log10_2hi + (n*log10_2lo + ivln10*log(x))
 *
 * Note 1:
 *	To guarantee log10(10**n)=n, where 10**n is normal, the rounding 
 *	mode must set to Round-to-Nearest.
 * Note 2:
 *	[1/log(10)] rounded to 53 bits has error  .198   ulps;
 *	log10 is monotonic at all binary break points.
 *
 * Special cases:
 *	log10(x) is NaN with signal if x < 0; 
 *	log10(+INF) is +INF with no signal; log10(0) is -INF with signal;
 *	log10(NaN) is that NaN with no signal;
 *	log10(10**N) = N  for N=0,1,...,22.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "support/log10.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace calculate_plot::support {

namespace {

const double
two54      =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
ivln10     =  4.34294481903251816668e-01, /* 0x3FDBCB7B, 0x1526E50E */
log10_2hi  =  3.01029995663611771306e-01, /* 0x3FD34413, 0x509F6000 */
log10_2lo  =  3.69423907715893078616e-13; /* 0x3D59FEF3, 0x11F12B36 */

// fdlibm's __HI(x) and __LO(x): the high and low 32 bits of a double (read and written through
// its bytes, whatever the byte order)
std::int32_t highWord(double x) {
    std::uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    return static_cast<std::int32_t>(u >> 32);
}
std::uint32_t lowWord(double x) {
    std::uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    return static_cast<std::uint32_t>(u);
}
double withHighWord(double x, std::int32_t hx) {
    std::uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    u = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(hx)) << 32) | (u & 0xffffffffu);
    std::memcpy(&x, &u, sizeof u);
    return x;
}

}  // namespace

double log10(double x)
{
	double y,z;
	std::int32_t i,k,hx;
	std::uint32_t lx;
	volatile double zero = 0.0;

	hx = highWord(x);	/* high word of x */
	lx = lowWord(x);	/* low word of x */

        k=0;
        if (hx < 0x00100000) {                  /* x < 2**-1022  */
            if (((hx&0x7fffffff)|lx)==0)
                return -two54/zero;             /* log(+-0)=-inf */
            if (hx<0) return (x-x)/zero;        /* log(-#) = NaN */
            k -= 54; x *= two54; /* subnormal number, scale up x */
            hx = highWord(x);                /* high word of x */
        }
	if (hx >= 0x7ff00000) return x+x;
	k += (hx>>20)-1023;
	i  = static_cast<std::int32_t>((static_cast<std::uint32_t>(k)&0x80000000u)>>31);
        hx = (hx&0x000fffff)|((0x3ff-i)<<20);
        y  = static_cast<double>(k+i);
        x = withHighWord(x, hx);
	z  = y*log10_2lo + ivln10*std::log(x);
	return  z+y*log10_2hi;
}

float log10f(float x) {
    if (x == 0x1.fddcf4p-98f) return -0x1.d33a46p+4f;  // (log10 there is a hair above the midpoint of two floats)
    return float(log10(double(x)));
}

}  // namespace calculate_plot::support
