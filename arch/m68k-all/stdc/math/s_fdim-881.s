#    Copyright � 2018, The AROS Development Team. All rights reserved.
#    $Id$
#include "aros/m68k/asm.h"

	.text
	.balign 4
	.globl	AROS_SLIB_ENTRY(fdim881, STDC, 181)
	_FUNCTION(AROS_SLIB_ENTRY(fdim881, STDC, 181))
	
AROS_SLIB_ENTRY(fdim881, STDC, 181):
	fmove.s	%d0,%fp0
	fmove.s	%d1,%fp1
	fsub.x	%fp1,%fp0
	fabs.x	%fp0
	fmove.s	%fp0,%d0
	rts
