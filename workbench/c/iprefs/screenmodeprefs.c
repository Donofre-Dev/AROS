/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.

    Desc:
*/

/*********************************************************************************************/

#include "global.h"

#include <aros/debug.h>

#include <prefs/prefhdr.h>
#include <prefs/screenmode.h>

#include <intuition/iprefs.h>

static LONG stopchunks[] =
{
    ID_PREF, ID_SCRM
};

/*********************************************************************************************/

void ScreenModePrefs_Handler(STRPTR filename)
{
    struct IFFHandle *iff;
    struct ScreenModePrefs *smp;

    D(bug("In IPrefs:ScreenModePrefs_Handler\n"));
    D(bug("filename=%s\n",filename));

    iff = CreateIFF(filename, stopchunks, 1);
    
    if (iff) {
        while(ParseIFF(iff, IFFPARSE_SCAN) == 0) {
            smp = LoadChunk(iff, sizeof(struct ScreenModePrefs), MEMF_ANY);
            if (smp) {
                struct IScreenModePrefs i;
        
                i.smp_DisplayID = AROS_BE2LONG(smp->smp_DisplayID);
                i.smp_Width     = AROS_BE2WORD(smp->smp_Width);
                i.smp_Height    = AROS_BE2WORD(smp->smp_Height);
                i.smp_Depth     = AROS_BE2WORD(smp->smp_Depth);
                i.smp_Control   = AROS_BE2WORD(smp->smp_Control);
                D(bug("[ScreenModePrefs] ModeID: 0x%08lX, Size: %dx%d, Depth: %d, Control: 0x%08lX\n",
                      i.smp_DisplayID, i.smp_Width, i.smp_Height, i.smp_Depth, i.smp_Control));
        
                SetIPrefs(&i, sizeof(struct IScreenModePrefs), IPREFS_TYPE_SCREENMODE);
                FreeVec(smp);
            }
        }
        KillIFF(iff);
    }
}
