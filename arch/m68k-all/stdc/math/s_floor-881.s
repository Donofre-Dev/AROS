#    Copyright � 2018, The AROS Development Team. All rights reserved.
#    $Id$
#include "aros/m68k/asm.h"

	.text
	.balign 4
	.globl	AROS_SLIB_ENTRY(floor881, STDC, 165)
	_FUNCTION(AROS_SLIB_ENTRY(floor881, STDC, 165))
	
AROS_SLIB_ENTRY(floor881, STDC, 165):
	fmove.s	%d0,%fp0
	fmove.x	%fp0,%fp1
	fintrz.x	%fp0
	fcmp.x	%fp1,%fp0
	fbole	.floor881done
	fsub.s	#0x3f800000,%fp0
.floor881done:
	fmove.s	%fp0,%d0
	rts
