/*
    Copyright (C) 1995-96, The AROS Development Team. All rights reserved.

    Desc: Demo/test of diskfont/AvailFonts()
*/


#include <proto/diskfont.h>
#include <proto/exec.h>
#include <diskfont/diskfont.h>
#include <exec/memory.h>
#include <stdio.h>

#include <proto/dos.h>

#define DEBUG 1
#include <aros/debug.h>

struct Library *DiskfontBase;

int main(int argc, char ** argv)
{
    ULONG afshortage = 0;

    struct AvailFontsHeader *afh;
    struct AvailFonts *afptr;

    /* Try to guess how many bytes are needed */
    ULONG afsize        = 10000;

    if (!(DiskfontBase = OpenLibrary("diskfont.library", 0L)))
    {
        VPrintf ("Couldn't open diskfont.library\n", NULL);
        return (RETURN_FAIL);
    }

    do
    {
        afh = (struct AvailFontsHeader *)AllocMem(afsize, MEMF_ANY);
        if (afh)
        {
            afshortage = AvailFonts((STRPTR)afh, afsize, AFF_MEMORY|AFF_DISK);
            if (afshortage)
            {
                FreeMem(afh, afsize);
                afsize += afshortage;
                afh = (struct AvailFontsHeader*)(-1L);
            }
        }
    } while (afshortage && afh);

    if (afh)
    {
        /* Print some info about the fonts */
        UWORD count;

        Printf("Number of fonts found: %ld\n", (ULONG)afh->afh_NumEntries);

        /* Get pointer to the first AvailFonts item */
        afptr = (struct AvailFonts*)&afh[1];

        for (count = afh->afh_NumEntries; count; count --)
        {
            Printf ("[%ld] Font name: %-30.s Font YSize: %ld  Style: 0x%lx  Flags 0x%lx\n",
                    (ULONG)afptr->af_Type, afptr->af_Attr.ta_Name,
                    (ULONG)afptr->af_Attr.ta_YSize,
                    (ULONG)afptr->af_Attr.ta_Style,
                    (ULONG)afptr->af_Attr.ta_Flags);


            afptr ++;
        }
    }

    CloseLibrary(DiskfontBase);
    return (RETURN_OK);
}
