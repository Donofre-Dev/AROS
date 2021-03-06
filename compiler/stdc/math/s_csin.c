/*	$OpenBSD: s_csin.c,v 1.6 2013/07/03 04:46:36 espie Exp $	*/
/*
 * Copyright (c) 2008 Stephen L. Moshier <steve@moshier.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*							csin()
 *
 *	Complex circular sine
 *
 *
 *
 * SYNOPSIS:
 *
 * double complex csin();
 * double complex z, w;
 *
 * w = csin (z);
 *
 *
 *
 * DESCRIPTION:
 *
 * If
 *     z = x + iy,
 *
 * then
 *
 *     w = sin x  cosh y  +  i cos x sinh y.
 *
 * csin(z) = -i csinh(iz).
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       -10,+10      8400       5.3e-17     1.3e-17
 *    IEEE      -10,+10     30000       3.8e-16     1.0e-16
 * Also tested by csin(casin(z)) = z.
 *
 */

#include <float.h>
#include <complex.h>
#include "math.h"
#include "math_private.h"

double complex
csin(double complex z)
{

	/* csin(z) = -I * csinh(I * z) = I * conj(csinh(I * conj(z))). */
	z = csinh(CMPLX(cimag(z), creal(z)));
	return (CMPLX(cimag(z), creal(z)));

}

#if	(LDBL_MANT_DIG == DBL_MANT_DIG)
AROS_MAKE_ASM_SYM(typeof(csinl), csinl, AROS_CSYM_FROM_ASM_NAME(csinl), AROS_CSYM_FROM_ASM_NAME(csin));
AROS_EXPORT_ASM_SYM(AROS_CSYM_FROM_ASM_NAME(csinl));
#endif	/* LDBL_MANT_DIG == DBL_MANT_DIG */
