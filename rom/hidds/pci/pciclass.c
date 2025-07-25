/*
    Copyright (C) 2004-2025, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/oop.h>

#include <hidd/hidd.h>
#include <hidd/pci.h>
#include <hardware/pci.h>
#include <oop/oop.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>

#include "pci.h"

#define PCI_DISPLAY_FAILEDIRQS

static OOP_Object *FindBridge(OOP_Class *cl, OOP_Object *drv, UBYTE bus);
static void AssignIRQ(OOP_Class *cl, OOP_Object *drv,
    struct MinList *irq_routing, OOP_Object *pcidev);

/*
    Returns 0 for no device, 1 for non-multi device and 2 for
    a multifunction device

    cl points to the base pci class which is used to extract static data
    o  points to the driver class which is used to read from config space
*/
static int isPCIDeviceAvailable(OOP_Class *cl, OOP_Object *o, UBYTE bus, UBYTE dev, UBYTE sub)
{
    UWORD Vend;
    UBYTE Type;

    D(bug("[PCI] %s()\n", __func__);)
    
    Vend = HIDD_PCIDriver_ReadConfigWord(o, NULL, bus, dev, sub, PCICS_VENDOR);

    if ((Vend == 0xffff) || (Vend == 0x0000))
    {
        /* 0xffff is an invalid vendor ID, and so is 0x0000
         * (Well, actually 0x0000 belongs to Gammagraphx, but this really
         * clashes with multifunc device scanning, so lets just hope nobody
         * has a card from them :) )
         */

        return 0;
    }

    Type = HIDD_PCIDriver_ReadConfigByte(o, NULL, bus, dev, sub, PCICS_HEADERTYPE);

    if ((Type & PCIHT_MULTIFUNC) == PCIHT_MULTIFUNC)
        return 2;

    return 1;
}

static OOP_Object *InsertDevice(OOP_Class *cl, OOP_Object *driver,
                                                    ULONG *highBus, struct TagItem *devtags)
{
    struct pcibase *pciBase = (struct pcibase *)cl->UserData;
    OOP_Class *deviceClass;
    OOP_Object *pcidev;
    IPTR bridge, secbus;

    D(bug("[PCI] %s()\n", __func__);)

    OOP_GetAttr(driver, aHidd_PCIDriver_DeviceClass, (IPTR *)&deviceClass);

    D(bug("[PCI] %s:Using class @ 0x%p to create device instance\n", __func__, deviceClass);)

    pcidev = OOP_NewObject(deviceClass, NULL, devtags);
    if (pcidev)
    {
        OOP_GetAttr(pcidev, aHidd_PCIDevice_isBridge, &bridge);
        if (bridge)
        {
            OOP_GetAttr(pcidev, aHidd_PCIDevice_SecBus, &secbus);
            if (secbus > *highBus)
                *highBus = secbus;
        }
        
        /*
         * Device class is our private and derived from rootclass.
         * This makes casting to struct Node * safe.
         */
        ObtainSemaphore(&pciBase->psd.dev_lock);
        ADDTAIL(&pciBase->psd.devices, pcidev);
        ReleaseSemaphore(&pciBase->psd.dev_lock);
    }
    return pcidev;
}

/*
 * PCI::SetUpDriver(OOP_Object *driverObject)
 *
 * A new PCI hardware driver is being added to the PCI subsystem.
 * The PCI bus handled through driver added is scanned, and all available
 * PCI devices are added to the device chain.
 */
BOOL PCI__HW__SetUpDriver(OOP_Class *cl, OOP_Object *o,
    struct pHW_SetUpDriver *msg)
{
    OOP_Object *drv = msg->driverObject;
    ULONG highBus = 0;
    ULONG bus, dev, sub, type;

    struct MinList *irq_routing;

    struct TagItem devtags[] =
    {
        { aHidd_PCIDevice_Bus           , 0         },
        { aHidd_PCIDevice_Dev           , 0         },
        { aHidd_PCIDevice_Sub           , 0         },
        { aHidd_PCIDevice_Driver        , (IPTR)drv },
        { TAG_DONE                      , 0         }
    };

    D(bug("[PCI] %s()\n", __func__);)

    OOP_GetAttr(drv, aHidd_PCIDriver_IRQRoutingTable, (IPTR *)&irq_routing);

    D(bug("[PCI] %s: Adding Driver Instance @ 0x%p (class @ 0x%p)\n", __func__, drv, OOP_OCLASS(drv)));

    D(bug("[PCI] %s: Driver Instance IRQ routing table @ 0x%p\n", __func__, irq_routing));

    /*
     * Scan the whole PCI bus looking for devices available
     * There is no need for semaphore protected list operations at this
     * point, because the driver is still not public.
     */
    for (bus = 0; bus <= highBus; bus++)
    {
        D(bug("[PCI] %s: Scanning bus %d\n", __func__,bus));

        devtags[0].ti_Data = bus;

        for (dev=0; dev < 32; dev++)
        {
            devtags[1].ti_Data = dev;
            devtags[2].ti_Data = 0;

            /* Knock knock! Is any device here? */
            type = isPCIDeviceAvailable(cl, drv, bus, dev, 0);

            switch(type)
            {
            /* Regular device */
            case 1:
                InsertDevice(cl, msg->driverObject, &highBus, devtags);
                break;

            /* Cool! Multifunction device, search subfunctions then */
            case 2:
                InsertDevice(cl, msg->driverObject, &highBus, devtags);
                    
                for (sub=1; sub < 8; sub++)
                {
                    devtags[2].ti_Data = sub;
                    if (isPCIDeviceAvailable(cl, drv, bus, dev, sub)) {
                        InsertDevice(cl, msg->driverObject, &highBus, devtags);
                    }
                }
                break;
            }
        }
    }

    // Handle IRQ Routing
    {
        struct pcibase *pciBase = (struct pcibase *)cl->UserData;
        OOP_Object *pcidev;
        if (irq_routing != NULL)
        {
            D(bug("[PCI] %s: Checking IRQ routing for newly added devices\n", __func__));

            ForeachNode(&pciBase->psd.devices, pcidev)
            {
                AssignIRQ(cl, drv, irq_routing, pcidev);
            }
        }
#if defined(PCI_DISPLAY_FAILEDIRQS)
        /*
         * Print out a warning to the user, in case the interrupt line was not assigned by BIOS
         * and we have failed to find routing information to configure it.
         */
        ForeachNode(&pciBase->psd.devices, pcidev)
        {
            IPTR devVal1 = 0, devVal2 = 0;
            OOP_GetAttr(pcidev, aHidd_PCIDevice_INTLine, &devVal1);
            OOP_GetAttr(pcidev, aHidd_PCIDevice_isBridge, &devVal2);
            if (devVal1 == 255 && !devVal2)
            {
                IPTR devVal3 = 0;
                OOP_GetAttr(pcidev, aHidd_PCIDevice_Bus, &devVal1);
                OOP_GetAttr(pcidev, aHidd_PCIDevice_Dev, &devVal2);
                OOP_GetAttr(pcidev, aHidd_PCIDevice_Sub, &devVal3);
                bug("[PCI] WARNING: Device 0x%p @ %u:%u:%u Interrupt line is not assigned!\n", pcidev, devVal1, devVal2, devVal3);
                bug("[PCI] WARNING: This may cause it to freeze or malfunction when used!\n");
            }
        }
#endif
    }

    // Success - add the driver to the end of the drivers list
    return TRUE;
}

/* 
 * Assign and configure a device's IRQ(s), using details obtained
 * from routing tables.
 */
static void AssignIRQ(OOP_Class *cl, OOP_Object *drv,
    struct MinList *irq_routing, OOP_Object *pcidev)
{
    IPTR d, line;
    OOP_Object *bridge;
    BOOL irq_found = FALSE;
    IPTR bus, dev;
    struct PCI_IRQRoutingEntry *e;

    D(bug("[PCI] %s()\n", __func__);)

    OOP_GetAttr(pcidev, aHidd_PCIDevice_Driver, &d);
    OOP_GetAttr(pcidev, aHidd_PCIDevice_IRQLine, &line);

    if (d == (IPTR)drv && line != 0)
    {
        /* For the first loop iteration, it's simpler to consider the device it's own bridge! */
        bridge = pcidev;

        while (!irq_found)
        {
            OOP_GetAttr(bridge, aHidd_PCIDevice_Bus, &bus);
            OOP_GetAttr(bridge, aHidd_PCIDevice_Dev, &dev);

            D(bug("[PCI] %s: Looking for routing for device %02x and INT%c on bus %d\n", __func__, dev,
                'A' + line - 1, bus));

            ForeachNode(irq_routing, e)
            {
                if ((e->re_PCIBusNum == bus) && (e->re_PCIDevNum == dev)
                    && (e->re_IRQPin == line))
                {
                    struct TagItem attr[] =
                    {
                        {aHidd_PCIDevice_INTLine, e->re_IRQ},
                        {TAG_DONE, 0UL}
                    };

                    D(bug("[PCI] %s: Got a match. Setting INTLine to %d\n", __func__, e->re_IRQ));
                    OOP_SetAttrs(pcidev, attr);
                    irq_found = TRUE;
                }
            }

            if (!irq_found)
            {
                D(bug("[PCI] %s: No match on bus %d. Trying parent bridge...\n", __func__, bus));

                /* We have to look for a routing entry that matches the parent bridge instead, so first find the
                   bridge */
                bridge = FindBridge(cl, drv, bus);
                if (!bridge)
                {
                    IPTR tmp;
                    OOP_GetAttr(pcidev, aHidd_PCIDevice_Bus, &tmp);
                    OOP_GetAttr(pcidev, aHidd_PCIDevice_Dev, &dev);
                    bug("[PCI] %s: No match on bus %d and no bridge. "
                        "Failing to re-assign INTLine for device  %02x on bus %d\n", __func__, bus, dev, tmp);
                    break;
                }
                OOP_GetAttr(bridge, aHidd_PCIDevice_Bus, &bus);

                /* Swizzle the INT pin as we traverse up to the parent bridge */
                D(bug("[PCI] %s:Swizzling the IRQPin from INT%c to INT%c\n", __func__, 'A' + line - 1,
                    'A' + (line - 1 + dev) % 4));
                line = (line - 1 + dev) % 4 + 1;
            }
        }
    }
}

/* Find the bridge that links to the given secondary bus */
static OOP_Object *FindBridge(OOP_Class *cl, OOP_Object *drv, UBYTE bus)
{
    struct pcibase *pciBase = (struct pcibase *)cl->UserData;
    OOP_Object *bridge = NULL, *pcidev;
    IPTR secbus, d, is_bridge;

    D(bug("[PCI] %s()\n", __func__);)

    ForeachNode(&pciBase->psd.devices, pcidev)
    {
        OOP_GetAttr(pcidev, aHidd_PCIDevice_Driver, &d);
        OOP_GetAttr(pcidev, aHidd_PCIDevice_isBridge, &is_bridge);
        
        if (d == (IPTR)drv && is_bridge)
        {
            OOP_GetAttr(pcidev, aHidd_PCIDevice_SecBus, &secbus);

            if (secbus == bus)
            {
                D(
                    IPTR bbus, dev, sub;
                    OOP_GetAttr(pcidev, aHidd_PCIDevice_Bus, &bbus);
                    OOP_GetAttr(pcidev, aHidd_PCIDevice_Dev, &dev);
                    OOP_GetAttr(pcidev, aHidd_PCIDevice_Sub, &sub);
                    bug("[PCI] %s: Found PCI-PCI bridge at %x:%02x.%x (%p)\n",
                        __func__, bbus, dev, sub, pcidev);
                )
                bridge = pcidev;
            }
        }
    }

    return bridge;
}

static const UBYTE attrTable[] =
{
    aoHidd_PCIDevice_VendorID,
    aoHidd_PCIDevice_ProductID,
    aoHidd_PCIDevice_RevisionID,
    aoHidd_PCIDevice_Interface,
    aoHidd_PCIDevice_Class,
    aoHidd_PCIDevice_SubClass,
    aoHidd_PCIDevice_SubsystemVendorID,
    aoHidd_PCIDevice_SubsystemID,
    aoHidd_PCIDevice_Driver
};

/*****************************************************************************************

    NAME
        moHidd_PCI_EnumDevices

    SYNOPSIS
        void OOP_DoMethod(OOP_Object *obj, struct pHidd_PCI_EnumDrivers *Msg);

        void HIDD_PCI_EnumDevices(OOP_Object *obj, struct Hook *callback,
                                  const struct TagItem *requirements);

    LOCATION
        CLID_Hidd_PCI

    FUNCTION
        This method calls the callback hook for every PCI device in the system
        that meets requirements specified (or every device if tags=NULL). It
        iterates not only through one PCI bus, but instead through all buses
        managed by all drivers present in the system.

    INPUTS
        obj          - A PCI subsystem object.
        callback     - A user-supplied hook which will be called for every device.
        requirements - A TagList specifying search parameters.

        The hook will be called with the following parameters:
            AROS_UFHA(struct Hook *, hook        , A0)
                - A pointer to hook structure itself
            AROS_UFHA(OOP_Object * , deviceObject, A2)
                - A PCI device object
            AROS_UFHA(APTR         , unused     , A1)
                - Not used
        
        The following tags are accepted as search parameters:
            tHidd_PCI_VendorID          - vendor ID
            tHidd_PCI_ProductID         - product ID
            tHidd_PCI_RevisionID        - revision ID
            tHidd_PCI_Interface         - PCI interface ID
            tHidd_PCI_Class             - PCI class ID
            tHidd_PCI_SubClass          - PCI subclass ID
            tHidd_PCI_SubsystemVendorID - subsystem vendor ID
            tHidd_PCI_SubsystemID       - subsystem ID
            tHidd_PCI_Driver            - a pointer to bus driver object [V4]

    RESULT
        None.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO

    INTERNALS

*****************************************************************************************/

void PCI__Hidd_PCI__EnumDevices(OOP_Class *cl, OOP_Object *o, struct pHidd_PCI_EnumDevices *msg)
{
    struct pcibase *pciBase = (struct pcibase *)cl->UserData;
    struct TagItem *tstate = (struct TagItem *)msg->requirements;
    struct TagItem *tag;
    IPTR matchVal[sizeof(attrTable)];
    ULONG i;
    OOP_Object *dev;
    BOOL ok;

    D(bug("[PCI] %s()\n", __func__);)

    for (i = 0; i < sizeof(attrTable); i++)
    {
        matchVal[i] = ~0;
    }

    /* Get requirements */
    while ((tag = NextTagItem(&tstate)))
    {
        ULONG idx = tag->ti_Tag - TAG_USER;

        if (idx < sizeof(attrTable))
            matchVal[idx] = tag->ti_Data;
    }

    /* Lock devices list for shared use */
    ObtainSemaphoreShared(&pciBase->psd.dev_lock);

    /* For every device in the system... */
    ForeachNode(&pciBase->psd.devices, dev)
    {
        /* check the requirements with its properties */
        ok = TRUE;

        for (i = 0; i < sizeof(attrTable); i++)
        {
            if (matchVal[i] != ~0)
            {
                IPTR value;

                OOP_GetAttr(dev, pciBase->psd.hiddPCIDeviceAB + attrTable[i], &value);
                ok &= (value == matchVal[i]);
            }
        }

        /* If requirements met, call Hook */
        if (ok)
        {
            CALLHOOKPKT(msg->callback, dev, NULL);
        }
    }

    ReleaseSemaphore(&pciBase->psd.dev_lock);
}

BOOL PCI__HW__RemoveDriver(OOP_Class *cl, OOP_Object *o, struct pHW_RemoveDriver *msg)
{
    struct pcibase *pciBase = (struct pcibase *)cl->UserData;
    OOP_Object *dev, *next, *drv;
    IPTR disallow = 0;

    D(
        bug("[PCI] %s()\n", __func__);
        bug("[PCI] %s: Removing hardware driver 0x%p\n", __func__, msg->driverObject);
    )

    /*
     * Get exclusive lock on devices list.
     * If we cannot do this, then either enumeration is running or
     * another driver is being added. We simply cannot remove the driver
     * in this case.
     * Well, in the latter case we actually could remove our driver, but
     * i believe this is extremely rare situation.
     */
    if (!AttemptSemaphore(&pciBase->psd.dev_lock))
        return FALSE;

    /*
     * Now we can check if we can remove our devices.
     * We think we can remove them if nobody has owned any of them.
     * Drivers which behave badly will not own devices, or they will
     * defer owning after enumeration loop has ended. So, removing a
     * driver is still very dangerous.
     * This can be improved if we implement map/unmnap and
     * AddInterrupt/RemoveInterrupt accounting in our drivers. The
     * driver would allow to expunge itself only if its internal counter
     * of used resources is zero.
     * PCI API wrappers (like prometheus.library) also build their
     * own reflection of devices list, so we will have to implement either
     * some ways to disable expunging, or (better) to get notifications
     * about devices list being updated. With this notification we can
     * have full hotplug support.
     */
    ForeachNode(&pciBase->psd.devices, dev)
    {
        OOP_GetAttr(dev, aHidd_PCIDevice_Driver, (IPTR *)&drv);
        if (drv == msg->driverObject)
        {
            IPTR owner;

            OOP_GetAttr(dev, aHidd_PCIDevice_Owner, &owner);
            disallow |= owner;
        }
    }

    if (disallow)
    {
        ReleaseSemaphore(&pciBase->psd.dev_lock);
        D(bug("[PCI] %s: failed, driver in use\n", __func__));
        return FALSE;
    }

    ForeachNodeSafe(&pciBase->psd.devices, dev, next)
    {
        REMOVE(dev);
        OOP_DisposeObject(dev);
    }

    ReleaseSemaphore(&pciBase->psd.dev_lock);
    D(bug("[PCI] %s: succeeded\n", __func__));
    return OOP_DoSuperMethod(cl, o, &msg->mID);
}

/*****************************************************************************************

    NAME
        moHidd_PCI_AddHardwareDriver

    SYNOPSIS
        OOP_Object *OOP_DoMethod(OOP_Object *obj, struct pHidd_PCI_AddHardwareDriver *Msg);

        OOP_Object *HIDD_PCI_AddHardwareDriver(OOP_Object *obj, OOP_Class *driverClass);

    LOCATION
        CLID_Hidd_PCI

    FUNCTION
        Creates a bus driver object and registers it in the system.

        Since V4 this interface is obsolete and deprecated. Use moHW_AddDriver
        method in order to install the driver.

    INPUTS
        obj         - A PCI subsystem object.
        driverClass - A pointer to OOP class of the driver. In order to create an object
                      of some previously registered public class, use
                      oop.library/OOP_FindClass().
        instanceTags - Tags used during driver instance creation.

    RESULT
        None.

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        moHidd_PCI_RemHardwareDriver

    INTERNALS

*****************************************************************************************/

void PCI__Hidd_PCI__AddHardwareDriver(OOP_Class *cl, OOP_Object *o,
                                      struct pHidd_PCI_AddHardwareDriver *msg)
{
    HW_AddDriver(o, msg->driverClass, msg->instanceTags);
}

AROS_UFH3(static BOOL, searchFunc,
    AROS_UFHA(struct Hook *, h,  A0),
    AROS_UFHA(OOP_Object *, driverObject, A2),
    AROS_UFHA(OOP_Class *, driverClass, A1))
{
    AROS_USERFUNC_INIT

    D(bug("[PCI] %s()\n", __func__);)

    if (OOP_OCLASS(driverObject) == driverClass)
    {
        h->h_Data = driverObject;
        return TRUE;
    }

    return FALSE;

    AROS_USERFUNC_EXIT
}

/*****************************************************************************************

    NAME
        moHidd_PCI_RemHardwareDriver

    SYNOPSIS
        void OOP_DoMethod(OOP_Object *obj, struct pHidd_PCI_RemHardwareDriver *Msg);

        void HIDD_PCI_RemHardwareDriver(OOP_Object *obj, OOP_Class *driverClass);

    LOCATION
        CLID_Hidd_PCI

    FUNCTION
        Unregisters and disposes bus driver objects of the given class.

        Since V4 this interface is obsolete and deprecated. Use moHW_RemoveDriver
        method in order to remove drivers.

    INPUTS
        obj         - A PCI subsystem object.
        driverClass - A pointer to a driver class.

    RESULT
        None

    NOTES

    EXAMPLE

    BUGS

    SEE ALSO
        moHidd_PCI_AddHardwareDriver

    INTERNALS

*****************************************************************************************/

BOOL PCI__Hidd_PCI__RemHardwareDriver(OOP_Class *cl, OOP_Object *o,
                                      struct pHidd_PCI_RemHardwareDriver *msg)
{
    BOOL ok = FALSE;
    struct Hook searchHook =
    {
        .h_Entry = (HOOKFUNC)searchFunc
    };

    D(bug("[PCI] %s()\n", __func__);)

    /*
     * A very stupid and slow algorithm.
     * Find a driver using Enum method, remember it, then remove.
     * Repeat until search succeeds.
     * We cannot remove drivers inside enumeration hook because EnumDrivers
     * locks internal objects list in shared mode. RemoveDriver locks the
     * same list in exclusive mode, and it's impossible to change semaphore's
     * mode on the fly.
     */
    do
    {
        searchHook.h_Data  = NULL;

        HW_EnumDrivers(o, &searchHook, msg->driverClass);

        if (searchHook.h_Data)
        {
            ok = HW_RemoveDriver(o, searchHook.h_Data);
            if (!ok)
                break;
        }
    } while (searchHook.h_Data);

    return ok;
}

OOP_Object *PCI__Root__New(OOP_Class *cl, OOP_Object *o, struct pRoot_New *msg)
{
    struct pcibase *pciBase = (struct pcibase *)cl->UserData;
    struct pci_staticdata *psd = &pciBase->psd;

    D(bug("[PCI] %s()\n", __func__);)

    if (!psd->pciObject)
    {
        struct TagItem new_tags[] =
        {
            {aHW_ClassName, (IPTR)"PCI Local Bus"},
            {TAG_DONE     , 0                }
        };
        struct pRoot_New new_msg =
        {
            .mID      = msg->mID,
            .attrList = new_tags
        };

        psd->pciObject = (OOP_Object *)OOP_DoSuperMethod(cl, o, &new_msg.mID);
    }
    return psd->pciObject;
}

VOID PCI__Root__Dispose(OOP_Class *cl, OOP_Object *o, OOP_Msg msg)
{

}
