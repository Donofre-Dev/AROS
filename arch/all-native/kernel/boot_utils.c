/*
    Copyright (C) 1995-2025, The AROS Development Team. All rights reserved.
*/

/* Utility functions for boot taglist relocation */

#include <proto/arossupport.h>
#include <utility/tagitem.h>

#include <string.h>

#include "boot_utils.h"
#include "tags.h"
#include "kernel_base.h"
#include "kernel_bootmem.h"

/* Our own block copier, because memcpy() can rely on CopyMem() which is not available yet */
void krnCopyMem(const void *src, void *dest, unsigned long size)
{
    const char *s = src;
    char *d = dest;
    unsigned long i;

    for (i = 0; i < size; i++)
        *d++ = *s++;
}

void RelocateBootMsg(const struct TagItem *msg)
{
    struct TagItem *dest;
    struct TagItem *tag;
    const struct TagItem *tstate = (const struct TagItem *)msg;
    ULONG num = 1;

    /* First count how much memory we will need */
    while ((tag = krnNextTagItem(&tstate)))
    {
        num++;
    }

    /* Allocate the memory */
    dest = krnAllocBootMem(num * sizeof(struct TagItem));
    BootMsg = dest;

    /* Now copy tagitems */
    tstate = (struct TagItem *)msg;
    while ((tag = krnNextTagItem(&tstate)))
    {
        dest->ti_Tag  = tag->ti_Tag;
        dest->ti_Data = tag->ti_Data;
        dest++;
    }

    /* Make sure the list is terminated */
    dest->ti_Tag = TAG_DONE;
}

void RelocateTagData(struct TagItem *tag, unsigned long size)
{
    char *src = (char *)tag->ti_Data;
    unsigned char *dest;
    int offset = 0;

    /*
     * Inject a space at the start of the command line
     * if necessary, to make parsing switches easier
     */
    if ((tag->ti_Data == KRN_CmdLine) && (src[0] != ' '))
        offset = 1;

    dest = krnAllocBootMem(size + offset);

    tag->ti_Data = (IPTR)dest;
    krnCopyMem(src, (APTR)((IPTR)dest + offset), size);
    if (offset)
        dest[0] = ' ';
}

void RelocateStringData(struct TagItem *tag)
{
    unsigned int l = strlen((char *)tag->ti_Data) + 1;

    RelocateTagData(tag, l);
}

void RelocateBSSData(struct TagItem *tag)
{
    struct KernelBSS *bss;
    unsigned int l = sizeof(struct KernelBSS);

    for (bss = (struct KernelBSS *)tag->ti_Data; bss->addr; bss++)
        l += sizeof(struct KernelBSS);

    RelocateTagData(tag, l);
}
