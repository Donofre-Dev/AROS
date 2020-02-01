/*
    Copyright © 1995-2020, The AROS Development Team. All rights reserved.
    $Id$

    Desc: AddBuffers CLI command
    Lang: English
*/


/******************************************************************************

    NAME

        AddBuffers (drive) [(N)]

    SYNOPSIS

        DRIVE/A, BUFFERS/N

    LOCATION

        C:

    FUNCTION

        Add buffers to the list of available buffers for a specific
	drive. Adding buffers speeds disk access but has the drawback
	of using up system memory (typically 512 bytes per buffer).
	Specifying a negative number subtracts buffers from the drive.
	    If only the DRIVE argument is specified, the number of 
	buffers for that drive are displayed without changing the buffer
	allocation.

    INPUTS

        DRIVE    --  the drive to alter the buffer allocation of
	BUFFERS  --  the number of buffers to add (or subtract in case of
	             a negative number) to a drive.

    RESULT

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

    HISTORY

******************************************************************************/

#define NO_INLINE_STDARG

#include <proto/dos.h>
#include <dos/dos.h>


const TEXT version[] = "$VER: AddBuffers 41.3 (" ADATE ")\n";

#define ARG_TEMPLATE "DRIVE/A,BUFFERS/N"

enum
{
    ARG_DRIVE = 0,
    ARG_BUFFERS,
    NOOFARGS
};


int __nocommandline = 1;

int main(void)
{
    IPTR           args[NOOFARGS] = { (IPTR)NULL, (IPTR)0 };
    struct RDArgs *rda;

    LONG return_code = RETURN_OK;
    LONG error = 0;
    ULONG buffers = 0;

    rda = ReadArgs(ARG_TEMPLATE, args, NULL);

    if (rda != NULL)
    {
	STRPTR  drive = (STRPTR)args[ARG_DRIVE];
	ULONG  *bufsptr = (ULONG *)args[ARG_BUFFERS];

	if (bufsptr != NULL)
	{
	    buffers = *bufsptr;
	}

	if(AddBuffers(drive, buffers))
	{
	    IPTR printargs[] =
	    {
		(IPTR)drive,
		IoErr()
	    };
	    VPrintf("%s has %ld buffers\n", (RAWARG)printargs);
	}
	else
	{
	    error = IoErr();
	    return_code = RETURN_FAIL;
	}
	
	FreeArgs(rda);
    }
    else
    {
	error = IoErr();
	return_code = RETURN_FAIL;
    }
    
    if (error != 0)
	PrintFault(IoErr(), "AddBuffers");
    return return_code;
}
