#    Copyright � 2018, The AROS Development Team. All rights reserved.
#    $Id$
#include "aros/m68k/asm.h"

	.text
	.balign 4
	.globl	AROS_SLIB_ENTRY(sqrt881, STDC, 159)
	_FUNCTION(AROS_SLIB_ENTRY(sqrt881, STDC, 159))
	
AROS_SLIB_ENTRY(sqrt881, STDC, 159):
	fmove.s	%d0,%fp0
	fsqrt.x	%fp0
	fmove.s	%fp0,%d0
	rts
