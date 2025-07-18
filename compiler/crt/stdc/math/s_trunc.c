/* @(#)s_floor.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

__FBSDID("$FreeBSD: src/lib/msun/src/s_trunc.c,v 1.4 2008/02/22 02:27:34 das Exp $");

/*
 * trunc(x)
 * Return x rounded toward 0 to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to trunc(x).
 */

#include <float.h>
#include "math.h"
#include "math_private.h"

static const double huge = 1.0e300;

double
trunc(double x)
{
	int32_t i0,i1,j0;
	uint32_t i;
	EXTRACT_WORDS(i0,i1,x);
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
	    if(j0<0) { 	/* raise inexact if x != 0 */
		if(huge+x>0.0) {/* |x|<1, so return 0*sign(x) */
		    i0 &= 0x80000000U;
		    i1 = 0;
		}
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		if(huge+x>0.0) {	/* raise inexact flag */
		    i0 &= (~i); i1=0;
		}
	    }
	} else if (j0>51) {
	    if(j0==0x400) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((uint32_t)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    if(huge+x>0.0)		/* raise inexact flag */
		i1 &= (~i);
	}
	INSERT_WORDS(x,i0,i1);
	return x;
}

#if LDBL_MANT_DIG == DBL_MANT_DIG
AROS_MAKE_ASM_SYM(typeof(truncl), truncl, AROS_CSYM_FROM_ASM_NAME(truncl), AROS_CSYM_FROM_ASM_NAME(trunc));
AROS_EXPORT_ASM_SYM(AROS_CSYM_FROM_ASM_NAME(truncl));
#endif
