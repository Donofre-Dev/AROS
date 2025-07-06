/*
    Copyright � 2025, The AROS Development Team. All rights reserved.
    $Id$

    Desc: Stub ctype functions for inclusion in the linklib only.
		    used when inlines cannot be.

    Lang: english
*/

#define STDC_NOINLINE_CTYPE
#include <ctype.h>

int isupper(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISupper));
}

int islower(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISlower));
}

int isalpha(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISalpha));
}

int isdigit(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISdigit));
}

int isxdigit(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISxdigit));
}

int isspace(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISspace));
}

int isprint(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISprint));
}

int isgraph(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISgraph));
}

int isblank(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISblank));
}

int iscntrl(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_IScntrl));
}

int ispunct(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISpunct));
}

int isalnum(int c)
{
	return ((*__ctype_b_ptr)[((int) (c)) & 0xff] & (unsigned short int) (_ISalnum));
}

int toupper(int c)
{
    if (c >= 0 && c <= 255)
        return (*__ctype_toupper_ptr)[c];
    return c;  // Undefined in C, but pass through if out of range
}

int tolower(int c)
{
    if (c >= 0 && c <= 255)
        return (*__ctype_tolower_ptr)[c];
    return c;
}

/* POSIX.1-2008/XSI extensions that are provided in stdc.library */
int isascii(int c)
{
    return (c & ~0x7F) == 0;
}

int toascii(int c)
{
    return (c & 0x7F);
}
