/*
    Copyright � 1995-2025, The AROS Development Team. All rights reserved.
    $Id$

    Desc: common code for language description files
*/

#include <exec/types.h>
#include <aros/system.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <libraries/locale.h>

#include <proto/exec.h>
#include <aros/libcall.h>
#include <aros/asmcall.h>

#include <aros/debug.h>

AROS_LD1(STRPTR, getlangstring,
    AROS_LHA(ULONG, id, D0),
    struct LocaleBase *, LocaleBase, 9, language);

/* ----------------------------------------------------------------------- */

/* Arrays for character type/conversion */
extern const STRPTR ___strings[];

/* -------------------------------------------------------------------------
   Library definition, you should not need to change any of this.
 ------------------------------------------------------------------------- */

struct Language
{
    struct Library library;
    struct ExecBase *sysbase;
    BPTR seglist;
};

extern const UBYTE name[];
extern const UBYTE version[];
extern const APTR inittabl[4];
extern void *const functable[];
AROS_UFP3(struct Language *, AROS_SLIB_ENTRY(init,language,0),
    AROS_UFPA(struct Language *, language, D0),
    AROS_UFPA(BPTR,             segList, A0),
    AROS_UFPA(struct ExecBase *, _SysBase, A6));
AROS_LD1(struct Language *, open,
    AROS_LHA(ULONG, version, D0),
    struct Language *, language, 1, language);
AROS_LD0(BPTR, close, struct Language *, language, 2, language);
AROS_LD0(BPTR, expunge, struct Language *, language, 3, language);
AROS_LD0I(int, null, struct Language *, language, 0, language);
AROS_LD0(ULONG, mask, struct Language *, language, 5, language);
extern const char end;

int entry(void)
{
    return -1;
}

const struct Resident languageTag =
{
    RTC_MATCHWORD,
    (struct Resident *)&languageTag,
    (APTR)&end,
    RTF_AUTOINIT,
    LANGVER,
    NT_LIBRARY,
    -120,
    (STRPTR)name,
    (STRPTR)&version[7],
    (ULONG *)inittabl
};

const UBYTE name[]=LANGSTR ".language";
const UBYTE nativelang[]=NLANGTAG;                      /* N.B - MUST come before $VER: */
const UBYTE version[]=LANGTAG;

const ULONG datatable = 0;

const APTR inittabl[4] =
{
    (APTR)sizeof(struct Language),
    (APTR)functable,
    (APTR)&datatable,
    &AROS_SLIB_ENTRY(init,language,0)
};

struct ExecBase *SysBase;

AROS_UFH3(struct Language *, AROS_SLIB_ENTRY(init,language,0),
    AROS_UFHA(struct Language *, language, D0),
    AROS_UFHA(BPTR,             segList, A0),
    AROS_UFHA(struct ExecBase *, _SysBase, A6)
)
{
    AROS_USERFUNC_INIT

    /*
        You could just as easily do this bit as the InitResident()
        datatable, however this works just as well.
    */
    language->library.lib_Node.ln_Type = NT_LIBRARY;
    language->library.lib_Node.ln_Pri = -120;
    language->library.lib_Node.ln_Name = (char *)name;
    language->library.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    language->library.lib_Version = LANGVER;
    language->library.lib_Revision = LANGREV;
    language->library.lib_IdString = (APTR)&version[7];

    language->seglist = segList;
    language->sysbase = _SysBase;
    SysBase = _SysBase;

    /*
        Although it is unlikely, you would return NULL if you for some
        unknown reason couldn't open.
    */
    D(bug("GetLangStr: Loaded at address %p\n", &AROS_SLIB_ENTRY(getlangstring,language,9)));
    return language;

    AROS_USERFUNC_EXIT
}


AROS_LH1(struct Language *, open,
    AROS_LHA(ULONG, version, D0),
    struct Language *, language, 1, language)
{
    AROS_LIBFUNC_INIT
    language->library.lib_OpenCnt++;
    language->library.lib_Flags &= ~LIBF_DELEXP;

    /* Again return NULL if you could not open */
    return language;

    AROS_LIBFUNC_EXIT
}

AROS_LH0(BPTR, close, struct Language *, language, 2, language)
{
    AROS_LIBFUNC_INIT

    if(! --language->library.lib_OpenCnt)
    {
        /* Delayed expunge pending? */
        if(language->library.lib_Flags & LIBF_DELEXP)
        {
            /* Yes, expunge the library */
            return AROS_LC0(BPTR, expunge, struct Language *, language, 3, language);
        }
    }
    return BNULL;
    AROS_LIBFUNC_EXIT
}

AROS_LH0(BPTR, expunge, struct Language *, language, 3, language)
{
    AROS_LIBFUNC_INIT

    BPTR ret;
    if(language->library.lib_OpenCnt)
    {
        /* Can't expunge, we are still open */
        language->library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    Remove(&language->library.lib_Node);
    ret = language->seglist;

    FreeMem((UBYTE *)language - language->library.lib_NegSize,
            language->library.lib_PosSize + language->library.lib_NegSize);

    return ret;

    AROS_LIBFUNC_EXIT
}

AROS_LH0I(int, null, struct Language *, language, 0, language)
{
    AROS_LIBFUNC_INIT

    return 0;

    AROS_LIBFUNC_EXIT
}
