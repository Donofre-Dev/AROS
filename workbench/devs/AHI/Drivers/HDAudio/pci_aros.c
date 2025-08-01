/*
The contents of this file are subject to the AROS Public License Version 1.1 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
http://www.aros.org/license.html

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
ANY KIND, either express or implied. See the License for the specific language governing rights and
limitations under the License.

(C) Copyright 2010-2025 The AROS Dev Team
(C) Copyright 2009-2010 Stephen Jones.
(C) Copyright xxxx-2009 Davy Wentzler.

The Initial Developer of the Original Code is Davy Wentzler.

All Rights Reserved.
*/

#include <aros/debug.h>

#define __OOP_NOATTRBASES__

#include <config.h>

#include <utility/hooks.h>
#include <exec/interrupts.h>
#include <asm/io.h>
#include <oop/oop.h>
#include <hidd/pci.h>
#include <aros/asmcall.h>

#include <proto/oop.h>

#include "DriverData.h"
#include "pci_wrapper.h"


#define KPrintF kprintf

struct Library *OOPBase;


static OOP_AttrBase __IHidd_PCIDev;
static OOP_Object *pciobj;

static OOP_MethodID mid_RB;
static OOP_MethodID mid_RW;
static OOP_MethodID mid_RL;

static OOP_MethodID mid_WB;
static OOP_MethodID mid_WW;
static OOP_MethodID mid_WL;

static BOOL inthandler_added;

BOOL ahi_pci_init(struct DriverBase* AHIsubBase)
{
    OOPBase = OpenLibrary(AROSOOP_NAME, 0);
    if (OOPBase)
    {
        __IHidd_PCIDev = OOP_ObtainAttrBase(IID_Hidd_PCIDevice);
        if (__IHidd_PCIDev)
        {
            pciobj = OOP_NewObject(NULL, CLID_Hidd_PCI, NULL);

            if (pciobj)
            {
                mid_RB = OOP_GetMethodID(IID_Hidd_PCIDevice, moHidd_PCIDevice_ReadConfigByte);
                mid_RW = OOP_GetMethodID(IID_Hidd_PCIDevice, moHidd_PCIDevice_ReadConfigWord);
                mid_RL = OOP_GetMethodID(IID_Hidd_PCIDevice, moHidd_PCIDevice_ReadConfigLong);

                mid_WB = OOP_GetMethodID(IID_Hidd_PCIDevice, moHidd_PCIDevice_WriteConfigByte);
                mid_WW = OOP_GetMethodID(IID_Hidd_PCIDevice, moHidd_PCIDevice_WriteConfigWord);
                mid_WL = OOP_GetMethodID(IID_Hidd_PCIDevice, moHidd_PCIDevice_WriteConfigLong);

                return TRUE;
            }
        }
    }
    
    return FALSE; 
}

void ahi_pci_exit(void)
{
    if (pciobj)
        OOP_DisposeObject(pciobj);
    if (__IHidd_PCIDev)
        OOP_ReleaseAttrBase(IID_Hidd_PCIDevice);
    if (OOPBase)
        CloseLibrary(OOPBase);
}

struct enum_data
{
    OOP_Object *prev_dev;
    OOP_Object *found_dev;
};

static AROS_UFH3(void, Enumerator,
         AROS_UFHA(struct Hook *,    hook,   A0),
         AROS_UFHA(OOP_Object *,     device, A2),
         AROS_UFHA(APTR,             msg,    A1))
{
    AROS_USERFUNC_INIT

    struct enum_data *ed = (struct enum_data *)hook->h_Data;

    if ((ed->found_dev == 0) && (device != ed->prev_dev))
    {
        ed->found_dev = device;
    }

    AROS_USERFUNC_EXIT
}


APTR ahi_pci_find_device(ULONG vendorid, ULONG deviceid, APTR dev)
{ 
    struct enum_data ed;

    struct Hook FindHook =
    {
        .h_Entry    = (HOOKFUNC)Enumerator,
        .h_Data     = &ed,
    };

    struct TagItem Reqs[] =
    {
#if 0
        { tHidd_PCI_Class       , 0x04 	    }, /* Multimedia */
        { tHidd_PCI_SubClass    , 0x01 	    }, /* Audio */
#endif
        { tHidd_PCI_VendorID    , vendorid  },
        { tHidd_PCI_ProductID   , deviceid  },
        { TAG_DONE              , 0   	    },
    };

    struct pHidd_PCI_EnumDevices enummsg =
    {
        .mID            = OOP_GetMethodID(CLID_Hidd_PCI, moHidd_PCI_EnumDevices),
        .callback       = &FindHook,
        .requirements   = (struct TagItem *)&Reqs,
    }, *msg = &enummsg;

    ed.prev_dev = (OOP_Object *)dev;
    ed.found_dev = 0;

    OOP_DoMethod(pciobj, (OOP_Msg)msg);

    //KPrintF("ahi_pci_find_device: found_dev = %lx\n", ed.found_dev);

    return (APTR)ed.found_dev;
}

ULONG pci_inl(ULONG addr, struct HDAudioChip *card)
{
    ULONG *real_addr = (ULONG *) ((IPTR)card->iobase + addr); // card->iobase should be virtual

    return *(real_addr);
}

UWORD pci_inw(ULONG addr, struct HDAudioChip *card)
{
    UWORD *real_addr = (UWORD *) ((IPTR)card->iobase + addr);

    return *(real_addr);
}

UBYTE pci_inb(ULONG addr, struct HDAudioChip *card)
{
    UBYTE *real_addr = (UBYTE *) ((IPTR)card->iobase + addr);

    return *(real_addr);
}

void pci_outl(ULONG value, ULONG addr, struct HDAudioChip *card)
{
    *((ULONG *) ((IPTR)card->iobase + addr)) = value;  
}

void pci_outw(UWORD value, ULONG addr, struct HDAudioChip *card)
{
    *((UWORD *) ((IPTR)card->iobase + addr)) = value;
}

void pci_outb(UBYTE value, ULONG addr, struct HDAudioChip *card)
{
    *((UBYTE *) ((IPTR)card->iobase + addr)) = value;
}


void outb_setbits(UBYTE value, ULONG addr, struct HDAudioChip *card)
{
    UBYTE data = pci_inb(addr, card);
    data |= value;

    pci_outb(data, addr, card);
}


void outb_clearbits(UBYTE value, ULONG addr, struct HDAudioChip *card)
{
    UBYTE data = pci_inb(addr, card);
    data &= ~value;

    pci_outb(data, addr, card);
}


void outw_setbits(UWORD value, ULONG addr, struct HDAudioChip *card)
{
    UWORD data = pci_inw(addr, card);
    data |= value;

    pci_outw(data, addr, card);
}


void outw_clearbits(UWORD value, ULONG addr, struct HDAudioChip *card)
{
    UWORD data = pci_inw(addr, card);
    data &= ~value;

    pci_outw(data, addr, card);
}


void outl_setbits(ULONG value, ULONG addr, struct HDAudioChip *card)
{
    ULONG data = pci_inl(addr, card);
    data |= value;

    pci_outl(data, addr, card);
}


void outl_clearbits(ULONG value, ULONG addr, struct HDAudioChip *card)
{
    ULONG data = pci_inl(addr, card);
    data &= ~value;

    pci_outl(data, addr, card);
}


ULONG inl_config(UBYTE reg, APTR dev)
{
    struct pHidd_PCIDevice_ReadConfigLong  msg;

    msg.mID = mid_RL;
    msg.reg = reg;

    return OOP_DoMethod((OOP_Object *)dev, (OOP_Msg)&msg);
}

UWORD inw_config(UBYTE reg, APTR dev)
{
    struct pHidd_PCIDevice_ReadConfigWord  msg;

    msg.mID = mid_RW;
    msg.reg = reg;

    return OOP_DoMethod((OOP_Object *)dev, (OOP_Msg)&msg);
}

UBYTE inb_config(UBYTE reg, APTR dev)
{
    struct pHidd_PCIDevice_ReadConfigByte  msg;

    msg.mID = mid_RB;
    msg.reg = reg;

    return OOP_DoMethod(dev, (OOP_Msg)&msg);
}

void outl_config(UBYTE reg, ULONG val, APTR dev)
{
    struct pHidd_PCIDevice_WriteConfigLong msg;

    msg.mID = mid_WL;
    msg.reg = reg;
    msg.val = val;

    OOP_DoMethod((OOP_Object *)dev, (OOP_Msg)&msg);
}

void outw_config(UBYTE reg, UWORD val, APTR dev)
{
    struct pHidd_PCIDevice_WriteConfigWord msg;

    msg.mID = mid_WW;
    msg.reg = reg;
    msg.val = val;

    OOP_DoMethod((OOP_Object *)dev, (OOP_Msg)&msg);
}

void outb_config(UBYTE reg, UBYTE val, APTR dev)
{
    struct pHidd_PCIDevice_WriteConfigByte msg;

    msg.mID = mid_WB;
    msg.reg = reg;
    msg.val = val;

    OOP_DoMethod((OOP_Object *)dev, (OOP_Msg)&msg);
}

ULONG ahi_pci_get_irq(APTR dev)
{
    IPTR val;

    OOP_GetAttr((OOP_Object *)dev, aHidd_PCIDevice_INTLine, &val);

    return (ULONG)val;
}

BOOL ahi_pci_add_intserver(struct Interrupt *i, APTR dev)
{

    D(bug("[HDAudio] %s(0x%p, 0x%p)\n", __func__, dev, i));
    if (!inthandler_added)
    {
        OOP_Object *driver;
        OOP_GetAttr((OOP_Object *)dev, aHidd_PCIDevice_Driver, (IPTR *) &driver);
        inthandler_added = (BOOL)HIDD_PCIDriver_AddInterrupt(driver, dev, i);
        return inthandler_added;
    }
    return FALSE;
}

void ahi_pci_rem_intserver(struct Interrupt *i, APTR dev)
{    
    D(bug("[HDAudio] %s(0x%p, 0x%p)\n", __func__, dev, i));

    if (inthandler_added)
    {
        OOP_Object *driver;

        OOP_GetAttr((OOP_Object *)dev, aHidd_PCIDevice_Driver, (IPTR *) &driver);
        HIDD_PCIDriver_RemoveInterrupt(driver, dev, i);

        inthandler_added = FALSE;
    }   
}


APTR ahi_pci_get_base_address(WORD which, APTR dev)
{
    OOP_AttrID attr = 0;
    IPTR val = 0;

    switch(which)
    {
        case 0:
          attr = aHidd_PCIDevice_Base0;
          break;
            
        case 1:
          attr = aHidd_PCIDevice_Base1;
          break;
            
        case 2:
          attr = aHidd_PCIDevice_Base2;
          break;
            
        case 3:
          attr = aHidd_PCIDevice_Base3;
          break;
            
        case 4:
          attr = aHidd_PCIDevice_Base4;
          break;
            
        case 5:
          attr = aHidd_PCIDevice_Base5;
          break;
            
        default:
          return 0;
    }

    OOP_GetAttr((OOP_Object *)dev, attr, &val);

    D(bug("[HDAudio] ahi_pci_get_base_address. Result %lx\n", val));

    return (APTR)val;
}

ULONG ahi_pci_get_base_size(WORD which, APTR dev)
{
    OOP_AttrID attr = 0;
    IPTR val = 0;

    switch(which)
    {
        case 0:
          attr = aHidd_PCIDevice_Size0;
          break;
            
        case 1:
          attr = aHidd_PCIDevice_Size1;
          break;
            
        case 2:
          attr = aHidd_PCIDevice_Size2;
          break;
            
        case 3:
          attr = aHidd_PCIDevice_Size3;
          break;
            
        case 4:
          attr = aHidd_PCIDevice_Size4;
          break;
            
        case 5:
          attr = aHidd_PCIDevice_Size5;
          break;
            
        default:
          return 0;
    }

    OOP_GetAttr((OOP_Object *)dev, attr, &val);

    D(bug("[HDAudio] ahi_pci_get_base_size. Result %lx\n", val));

    return (ULONG)val;

}



ULONG ahi_pci_get_type(WORD which, APTR dev)
{
  OOP_AttrID attr = 0;
  IPTR val = 0;
    
  switch(which)
  {
    case 0:
      attr = aHidd_PCIDevice_Type0;
      break;
        
    case 1:
      attr = aHidd_PCIDevice_Type1;
      break;
        
    case 2:
      attr = aHidd_PCIDevice_Type2;
      break;
        
    case 3:
      attr = aHidd_PCIDevice_Type3;
      break;
        
    case 4:
      attr = aHidd_PCIDevice_Type4;
      break;
        
    case 5:
      attr = aHidd_PCIDevice_Type5;
      break;
        
    default:
      return 0;
  }
    
  OOP_GetAttr((OOP_Object *)dev, attr, &val);

  D(bug("[HDAudio] ahi_pci_get_type. Result %lx\n", val));
    
  return (ULONG) val;
}
