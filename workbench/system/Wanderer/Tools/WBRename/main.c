/*
    Copyright (C) 2006-2011, The AROS Development Team. All rights reserved.
*/

#define MUIMASTER_YES_INLINE_STDARG

#define DEBUG 0
#include <aros/debug.h>

#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/workbench.h>
#include <proto/dos.h>
#include <proto/icon.h>

#include <dos/dos.h>
#include <libraries/mui.h>
#include <workbench/startup.h>

#include "locale.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char versionstring[] = "$VER: WBRename 0.6 (21.06.2024) \xA9 2006-2023 AROS Dev Team";

static STRPTR AllocateNameFromLock(BPTR lock);
static void bt_ok_hook_function(void);
static void Cleanup(STRPTR s);
static BOOL doRename(const STRPTR oldname, const STRPTR newname);
static BOOL doRelabel(const STRPTR oldname, const STRPTR newname);
static void MakeGUI(void);

static Object *app, *window, *bt_ok, *bt_cancel, *str_name;
static struct Hook bt_ok_hook;
static STRPTR oldname;
static TEXT str_line[256];
static STRPTR illegal_chars = "/:";
static BOOL isInfoFile = FALSE;
static BOOL isVolume = FALSE;

/* Cleaned up */
static BPTR oldlock = (BPTR)-1;
static STRPTR volname = NULL;
static STRPTR wbobjname = NULL;

static BOOL checkIfInfoFile(CONST_STRPTR path)
{
    return (strncmp(path + strlen(path) - 5, ".info", 5) == 0);
}

static BOOL checkIfVolume(BPTR lock, CONST_STRPTR path)
{
    BOOL _ret = FALSE;

    if (strcmp(path, "") == 0)
    {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        Examine(lock, fib);
        if (fib->fib_DirEntryType == ST_ROOT)
            _ret = TRUE;
        FreeDosObject(DOS_FIB, fib);
    }

    return _ret;
}

int main(int argc, char **argv)
{
    struct WBStartup *startup;
    BPTR parentlock = (BPTR)-1;

    if (argc != 0)
    {
        PutStr(_(MSG_WB_ONLY));
        Cleanup(NULL);
    }
    startup = (struct WBStartup *) argv;

    D(bug("[WBRename] Args %d\n", startup->sm_NumArgs));

    if (startup->sm_NumArgs != 2)
       Cleanup(_(MSG_NEEDS_MORE_ARGS));

    parentlock = startup->sm_ArgList[1].wa_Lock;
    oldname    = startup->sm_ArgList[1].wa_Name;

    if ((parentlock == BNULL) || (oldname == NULL))
       Cleanup(_(MSG_INVALID_LOCK));

    /* If renaming, parent lock is lock to parent directory of file/directory to be renamed */
    /* If relabeling, parent lock is lock to volume that is to be relabeled */

    wbobjname = AllocateNameFromLock(parentlock);

    if (checkIfVolume(parentlock, oldname))
    {
        isVolume = TRUE;
        volname = StrDup(wbobjname);
        volname[strlen(volname) - 1] = '\0'; /* Cut the ':' at end */
        oldname = volname;
    }

    if (!isVolume && checkIfInfoFile(oldname))
    {
        isInfoFile = TRUE;
        WORD len = strlen(oldname);
        oldname[len - 5] = '\0';
    }

    if (!isVolume)
        oldlock = CurrentDir(parentlock);
    
    MakeGUI();
    DoMethod(app, MUIM_Application_Execute);

    UpdateWorkbenchObject(wbobjname, WBDRAWER, TAG_DONE);

    if (isVolume)
        UpdateWorkbenchObject("", WBAPPICON, TAG_DONE); /* Hack to refresh desktop */

    Cleanup(NULL);
    return RETURN_OK;
}


static void MakeGUI(void)
{
    bt_ok_hook.h_Entry = (APTR)bt_ok_hook_function;
    sprintf(str_line, _(MSG_LINE), oldname);
    app = (Object *)ApplicationObject,
    MUIA_Application_Title      , __(MSG_TITLE),
    MUIA_Application_Version    , (IPTR) versionstring,
    MUIA_Application_Copyright  , __(MSG_COPYRIGHT),
    MUIA_Application_Author     , (IPTR) "The AROS Development Team",
    MUIA_Application_Description, __(MSG_DESCRIPTION),
    MUIA_Application_Base       , (IPTR) "WBRENAME",
    MUIA_Application_UseCommodities, FALSE,
    MUIA_Application_UseRexx, FALSE,
        SubWindow, (IPTR)(window = (Object *)WindowObject,
            MUIA_Window_Title, __(MSG_WINDOW_TITLE),
            MUIA_Window_ID, MAKE_ID('W','B','R','N'),
            MUIA_Window_Width, MUIV_Window_Width_Visible(33),  // Set width at least as 33% of visible screen
            MUIA_Window_NoMenus, TRUE,
            MUIA_Window_CloseGadget, FALSE,
            WindowContents, (IPTR) (VGroup,
            MUIA_Frame, MUIV_Frame_Group,
            Child, (IPTR) (HGroup,
                Child, (IPTR) HVSpace,
                    Child, (IPTR) TextObject,
                        MUIA_Text_PreParse, (IPTR) "\33r",
                        MUIA_Text_Contents, (IPTR) str_line,
                    End,
            End),
                Child, (IPTR) (HGroup,
                Child, (IPTR) Label2(__(MSG_NAME)),
                Child, (IPTR)(str_name = (Object *)StringObject,
                    MUIA_CycleChain, 1,
                    MUIA_String_Contents, (IPTR) oldname,
                    MUIA_String_MaxLen, MAXFILENAMELENGTH,
                    MUIA_String_Reject, (IPTR) illegal_chars, // Doesn't work :-/
                    MUIA_String_Columns, -1,
                    MUIA_Frame, MUIV_Frame_String,
                End),
            End),
            Child, (IPTR) (RectangleObject,
                MUIA_Rectangle_HBar, TRUE,
                MUIA_FixHeight,      2,
            End),
            Child, (IPTR) (HGroup,
                Child, (IPTR) (bt_ok = ImageButton(__(MSG_OK), "THEME:Images/Gadgets/OK")),
                Child, (IPTR) (bt_cancel = ImageButton(__(MSG_CANCEL),"THEME:Images/Gadgets/Cancel")),
            End),
            End),
        End),
    End;
    
    if (!app)
        Cleanup(_(MSG_FAILED_CREATE_APP));

    DoMethod(window, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(bt_ok, MUIM_Notify, MUIA_Pressed, FALSE,
        app, 2, MUIM_CallHook, (IPTR)&bt_ok_hook);
    set(window, MUIA_Window_Open, TRUE);
    set(window, MUIA_Window_ActiveObject, str_name);
    set(window, MUIA_Window_DefaultObject, bt_ok);
}


static void bt_ok_hook_function(void)
{
    STRPTR newname = (STRPTR)XGET(str_name, MUIA_String_Contents);
    D(bug("WBRename oldname %s newname %s \n", oldname, newname));
    BOOL success = FALSE;

    if (isVolume)
        success = doRelabel(oldname, newname);
    else
        success = doRename(oldname, newname);

    if (success)
    {
       DoMethod(app, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    }
    else
    {
        set(window, MUIA_Window_Activate, TRUE);
        set(window, MUIA_Window_ActiveObject, str_name);
    }

}

static BOOL canRename(const STRPTR oldname, const STRPTR newname)
{
    BPTR newl;
    if ((newl = Lock(newname, ACCESS_READ)))
    {
        /* There is already a file with new name. Let's see if it is the same file as oldname */
        BPTR oldl;
        if (oldl = Lock(oldname, ACCESS_READ))
        {
            LONG res = SameLock(newl, oldl);
            UnLock(oldl);
            UnLock(newl);

            if (res == LOCK_SAME)
            {
                /* This is a case of changing capilatization of name. It's ok */
                return TRUE;
            }
            else
                return FALSE;
        }
        else
        {
            /* Old file no longer exists? Return TRUE and let the Rename call fail to show correct error */
            UnLock(newl);
            return TRUE;
        }
    }

    return TRUE;
}

static STRPTR getDevNameForVolume(STRPTR volumeName)
{
    struct DosList *dl;
    STRPTR devName = NULL;

    /* Get the work drives device name */
    dl = LockDosList(LDF_VOLUMES | LDF_READ);
    dl = FindDosEntry(dl, volumeName, LDF_VOLUMES | LDF_READ);
    UnLockDosList(LDF_VOLUMES | LDF_READ);

    if (dl != NULL)
    {
        APTR voltask = dl->dol_Task;
        dl = LockDosList(LDF_DEVICES|LDF_READ);
        if (dl) {
            while((dl = NextDosEntry(dl, LDF_DEVICES)))
            {
                if (dl->dol_Task == voltask)
                {
                    devName = AROS_BSTR_ADDR(dl->dol_Name);
                }
            }
            UnLockDosList(LDF_VOLUMES|LDF_READ);
        }
    }
    return devName;
}

static BOOL doRelabel(const STRPTR oldname, const STRPTR newname)
{
    TEXT buff[128];
    strcpy(buff, "");
    strcat(buff, oldname);
    strcat(buff, ":"); /* Volume name with ':' at end */
    STRPTR tmp = getDevNameForVolume(buff);
    strcpy(buff, "");
    strcat(buff, tmp);
    strcat(buff, ":"); /* Device name with ':' at end */

    return Relabel(buff, newname);
}

static BOOL doRename(const STRPTR oldname, const STRPTR newname)
{
    BOOL retval = FALSE;
    if (( oldname == NULL) || (newname == NULL))
       return retval;

    STRPTR oldinfoname = NULL;
    STRPTR newinfoname = NULL;
    BOOL infoexists=FALSE;
    BPTR test;

    oldinfoname = AllocVec(strlen(oldname) + 6, MEMF_ANY);
    if (!oldinfoname)
    {
        MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_OUTOFMEMORY));
        goto end;
    }
    strcpy(oldinfoname, oldname);
    strcat(oldinfoname, ".info");

    newinfoname = AllocVec(strlen(newname) + 6, MEMF_ANY);
    if (!newinfoname)
    {
        MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_OUTOFMEMORY));
        goto end;
    }
    strcpy(newinfoname, newname);
    strcat(newinfoname, ".info");

    if (strpbrk(newname, illegal_chars))
    {
        MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_ILLEGAL_CHARS), newname);
        goto end;
    }

    if (!isInfoFile)
    {
        if (!canRename(oldname, newname))
        {
            MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_ALREADY_EXIST), newname);
            goto end;
        }
    }
    else
    {
        if (!canRename(oldinfoname, newinfoname))
        {
            MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_ALREADY_EXIST), newname);
            goto end;
        }
    }

    if ((test = Lock(oldinfoname, ACCESS_READ)))
    {
        UnLock(test);
        infoexists = TRUE; // we have an .info file
        if (!canRename(oldinfoname, newinfoname))
        {
            MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_ALREADY_EXIST), newinfoname);
            goto end;
        }
    }

    if (!isInfoFile)
    {
        if (Rename(oldname, newname) == DOSFALSE)
        {
            MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_FAILED), oldname, GetDosErrorString(IoErr()));
            goto end;
        }
    }

    if (infoexists)
    {
        if ( Rename(oldinfoname, newinfoname) == DOSFALSE)
        {
            MUI_Request(app, window, 0, _(MSG_ERROR_TITLE), _(MSG_OK), _(MSG_FAILED), oldinfoname, GetDosErrorString(IoErr()));
            goto end;
        }
    }

    retval = TRUE;

end:
    FreeVec(oldinfoname);
    FreeVec(newinfoname);
    return retval;
}


static STRPTR AllocateNameFromLock(BPTR lock)
{
    ULONG  length = 512;
    STRPTR buffer = NULL;
    BOOL   done   = FALSE;

    while (!done)
    {
        FreeVec(buffer);
    
        buffer = AllocVec(length, MEMF_ANY);
        if (buffer != NULL)
        {
            if (NameFromLock(lock, buffer, length))
            {
                done = TRUE;
                break;
            }
            else
            {
                if (IoErr() == ERROR_LINE_TOO_LONG)
                {
                    length += 512;
                    continue;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            break;
        }
    }

    if (done)
    {
        return buffer;
    }
    else
    {
        FreeVec(buffer);
        return NULL;
    }
}


static void Cleanup(STRPTR s)
{
    MUI_DisposeObject(app);

    if (oldlock != (BPTR)-1)
        CurrentDir(oldlock);

    if (volname != NULL)
        FreeVec(volname);

    if (wbobjname != NULL)
        FreeVec(wbobjname);

    if (s)
    {
        if (IntuitionBase)
        {
            struct EasyStruct es;
            es.es_StructSize = sizeof(struct EasyStruct);
            es.es_Flags = 0;
            es.es_Title = _(MSG_ERROR_TITLE);
            es.es_TextFormat = s;
            es.es_GadgetFormat = _(MSG_OK);
            EasyRequest(NULL, &es, NULL, NULL);
        }
        else
        {
            PutStr(s);
        }
        exit(RETURN_ERROR);
    }
    exit(RETURN_OK);
}

