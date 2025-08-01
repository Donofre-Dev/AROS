/*
    Copyright (C) 2012-2025, The AROS Development Team. All rights reserved.
*/
#include <aros/symbolsets.h>

#include <exec/types.h>
#include <proto/locale.h>

#define CATCOMP_ARRAY
#include "strings.h"

#define CATALOG_NAME     "System/Tools/QuickPart.catalog"

#define CATALOG_VERSION  1

/*** Variables **************************************************************/
struct Catalog *catalog;


/*** Functions **************************************************************/
/* Main *********************************************************************/
CONST_STRPTR _(ULONG id)
{
    if (LocaleBase != NULL && catalog != NULL)
    {
        return GetCatalogStr(catalog, id, CatCompArray[id].cca_Str);
    }
    else
    {
        return CatCompArray[id].cca_Str;
    }
}

/* Setup ********************************************************************/
BOOL Locale_Initialize(VOID)
{
    if (LocaleBase != NULL)
    {
        catalog = OpenCatalog
        (
            NULL, CATALOG_NAME, OC_Version, CATALOG_VERSION, TAG_DONE
        );
    }
    else
    {
        catalog = NULL;
    }
    
    return TRUE;
}

VOID Locale_Deinitialize(VOID)
{
    if(LocaleBase != NULL && catalog != NULL) CloseCatalog(catalog);
}

ADD2INIT(Locale_Initialize,   90);
ADD2EXIT(Locale_Deinitialize, 90);
