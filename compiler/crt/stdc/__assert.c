/*
    Copyright (C) 1995-2018, The AROS Development Team. All rights reserved.

    C99 assert() stdc.library support
*/
#include <proto/exec.h>

#define __NOBLIBBASE__

#include <aros/arossupportbase.h>
#include <intuition/intuition.h>
#include <aros/debug.h>

#include <stdlib.h>

#include "__stdc_intbase.h"
#include "__optionallibs.h"

/*****************************************************************************

    NAME */
#include <assert.h>

        void __assert (

/*  SYNOPSIS */
        const char * expr,
        const char * file,
        unsigned int line)

/*  FUNCTION
        This is a function that is used for implementation of the C99 assert()
        function.

    INPUTS
        expr - The expression to evaluate. The type of the expression does
                not matter, only if its zero/NULL or not.
        file - Name of the source file.
        line - Line number of assert() call.

    RESULT
        The function doesn't return.

    NOTES
        Different versions of this function are available. This function
        is used when a program is using stdc.library but not
        stdcio.library or posixc.library.
        Because no normal DOS file I/O is available an attempt will be made
        to display the assertion in a requester and thus deviating from the
        C99 standard that says it to go to the error stream.

    EXAMPLE

    BUGS

    SEE ALSO
        assert()

    INTERNALS

******************************************************************************/
{
    struct StdCIntBase *StdCBase =
        (struct StdCIntBase *)__aros_getbase_StdCBase();

    if (__intuition_available(StdCBase))
    {
        struct EasyStruct es =
            {
                sizeof(struct EasyStruct),
                0,
                (CONST_STRPTR)"Failed Assertion",
                (CONST_STRPTR)"Assertion (%s) failed in %s:%u",
                (CONST_STRPTR)"OK"
            };

        stdcEasyRequest(StdCBase->StdCIntuitionBase, NULL, &es, NULL, expr, file, line);
    }
    else
        kprintf("Assertion (%s) failed in %s:%u", expr, file, line);

    abort();
}
