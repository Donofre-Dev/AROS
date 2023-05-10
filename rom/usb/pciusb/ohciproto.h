#ifndef OHCIPROTO_H
#define OHCIPROTO_H

#include "ohcichip.h"

void ohciUpdateFrameCounter(struct PCIController *hc);
void ohciAbortRequest(struct PCIController *hc, struct IOUsbHWReq *ioreq);
BOOL ohciInit(struct PCIController *hc, struct PCIUnit *hu);
void ohciFree(struct PCIController *hc, struct PCIUnit *hu);

WORD ohciInitIsochIO(struct PCIController *hc, struct RTIsoNode *rtn);
WORD ohciQueueIsochIO(struct PCIController *hc, struct RTIsoNode *rtn);
void ohciFreeIsochIO(struct PCIController *hc, struct RTIsoNode *rtn);
void ohciStartIsochIO(struct PCIController *hc, struct RTIsoNode *rtn);
void ohciStopIsochIO(struct PCIController *hc, struct RTIsoNode *rtn);

BOOL ohciSetFeature(struct PCIUnit *unit, struct PCIController *hc, UWORD hciport, UWORD idx, UWORD val, WORD *retval);
BOOL ohciClearFeature(struct PCIUnit *unit, struct PCIController *hc, UWORD hciport, UWORD idx, UWORD val, WORD *retval);
BOOL ohciGetStatus(struct PCIController *hc, UWORD *mptr, UWORD hciport, UWORD idx, WORD *retval);

#ifdef base
#undef base
#endif
#define base (hc->hc_Device)
#if defined(AROS_USE_LOGRES)
#ifdef LogResBase
#undef LogResBase
#endif
#define LogResBase (base->hd_LogResBase)
#ifdef LogHandle
#undef LogHandle
#endif
#define LogHandle (hc->hc_LogRHandle)
#endif
/* /// "ohciAllocED()" */
static inline struct OhciED * ohciAllocED(struct PCIController *hc) {
    struct OhciHCPrivate *ohcihcp = (struct OhciHCPrivate *)hc->hc_CPrivate;
    struct OhciED *oed = ohcihcp->ohc_OhciEDPool;

    if(!oed)
    {
        // out of QHs!
        KPRINTF(200, "Out of EDs!\n");
        return NULL;
    }

    ohcihcp->ohc_OhciEDPool = oed->oed_Succ;
    return(oed);
}
/* \\\ */

/* /// "ohciFreeED()" */
static inline void ohciFreeED(struct PCIController *hc, struct OhciED *oed) {
    struct OhciHCPrivate *ohcihcp = (struct OhciHCPrivate *)hc->hc_CPrivate;
    oed->oed_HeadPtr = oed->oed_TailPtr;	// Protect against ocassional reuse
    CONSTWRITEMEM32_LE(&oed->oed_EPCaps, OECF_SKIP);
    SYNC;

    oed->oed_IOReq     = NULL;
    oed->oed_Buffer    = NULL;
    oed->oed_SetupData = NULL;
    oed->oed_Succ = ohcihcp->ohc_OhciEDPool;
    ohcihcp->ohc_OhciEDPool = oed;
}
/* \\\ */

/* /// "ohciAllocTD()" */
static inline struct OhciTD * ohciAllocTD(struct PCIController *hc) {
    struct OhciHCPrivate *ohcihcp = (struct OhciHCPrivate *)hc->hc_CPrivate;
    struct OhciTD *otd = ohcihcp->ohc_OhciTDPool;

    if(!otd)
    {
        // out of TDs!
        KPRINTF(200, "Out of TDs!\n");
        return NULL;
    }

    ohcihcp->ohc_OhciTDPool = otd->otd_Succ;
    return(otd);
}
/* \\\ */

/* /// "ohciFreeTD()" */
static inline void ohciFreeTD(struct PCIController *hc, struct OhciTD *otd) {
    struct OhciHCPrivate *ohcihcp = (struct OhciHCPrivate *)hc->hc_CPrivate;
    otd->otd_NextTD = 0; // Protect against looped TD list in ocassion of TD reuse ("Rogue TD" state)
    SYNC;

    otd->otd_ED = NULL;
    otd->otd_Succ = ohcihcp->ohc_OhciTDPool;
    ohcihcp->ohc_OhciTDPool = otd;
}
/* \\\ */

static inline void ohciDisableED(struct OhciED *oed)
{
    ULONG ctrlstatus;

    // disable ED
    ctrlstatus = READMEM32_LE(&oed->oed_EPCaps);
    ctrlstatus |= OECF_SKIP;
    WRITEMEM32_LE(&oed->oed_EPCaps, ctrlstatus);

    // unlink from schedule
    oed->oed_Succ->oed_Pred = oed->oed_Pred;
    oed->oed_Pred->oed_Succ = oed->oed_Succ;
    oed->oed_Pred->oed_NextED = oed->oed_Succ->oed_Self;
    oed->oed_IOReq = NULL;
    CacheClearE(&oed->oed_Pred->oed_EPCaps, 16, CACRF_ClearD);
    SYNC;
}

static inline void ohciDisableInt(struct PCIController *hc, ULONG mask)
{
    WRITEREG32_LE(hc->hc_RegBase, OHCI_INTDIS, mask);
    hc->hc_PCIIntEnMask &= ~mask;
}

static inline void ohciEnableInt(struct PCIController *hc, ULONG mask)
{
    WRITEREG32_LE(hc->hc_RegBase, OHCI_INTSTATUS, mask); // Clear potential dangling status
    hc->hc_PCIIntEnMask |= mask;
    WRITEREG32_LE(hc->hc_RegBase, OHCI_INTEN, mask);
}
#undef base
#if defined(AROS_USE_LOGRES)
#undef LogResBase
#undef LogHandle
#endif
#endif /* OHCIPROTO_H */
