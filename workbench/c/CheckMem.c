/*
    Copyright © 1995-2020, The AROS Development Team. All rights reserved.

    Desc: CheckMem CLI command
*/

/******************************************************************************


    NAME

        CheckMem

    SYNOPSIS

        (N/A)

    LOCATION

        C:

    FUNCTION

        Triggers a memory check.

    INPUTS

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

******************************************************************************/

#include <exec/memory.h>
#include <proto/exec.h>

const TEXT version[] = "$VER: CheckMem 40.0 (" ADATE ")\n";

int main(void)
{
    AvailMem(MEMF_CLEAR);
    
    return 0;
}
