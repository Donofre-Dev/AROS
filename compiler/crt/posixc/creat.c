/*
    Copyright (C) 1995-2025, The AROS Development Team. All rights reserved.

    POSIX.1-2008 function creat().
*/

/*****************************************************************************

    NAME */
#include <fcntl.h>

        int creat (

/*  SYNOPSIS */
        const char * pathname,
        int          mode)

/*  FUNCTION
        Creates a file with the specified mode and name.

    INPUTS
        pathname - Path and filename of the file you want to open.
        mode - The access flags.

    RESULT
        -1 for error or a file descriptor for use with write().

    NOTES
        If the filesystem doesn't allow to specify different access modes
        for users, groups and others, then the user modes are used.

        This is the same as open (pathname, O_CREAT|O_WRONLY|O_TRUNC, mode);

        This function must not be used in a shared library or
        in a threaded application.

    EXAMPLE

    BUGS

    SEE ALSO
        open(), close(), write(), __posixc_fopen()

    INTERNALS

******************************************************************************/
{
    return open (pathname, O_CREAT|O_WRONLY|O_TRUNC, mode);
} /* __posixc_creat */

