/*
    Copyright (C) 2010-2025, The AROS Development Team. All rights reserved.
*/

/*
    This is ment to be (in future) a generic class that will be capable of
    compositing bitmaps on screen to get effects like screen dragging.
    The code is generic where possible, using abstract OOP_Objects instead of
    concrete driver structures. There are places where nouveau specific calls
    are performed, however there are only few and can be generilized to
    (private) methods that should be reimplemented by child classes in each of
    the drivers.
*/

#define DEBUG 0

#include <aros/debug.h>
#include <proto/exec.h>
#include <proto/oop.h>
#include <proto/utility.h>

/* Non generic part */
#include "intelgma_hidd.h"
/* Non generic part */

#include "intelgma_compositor.h"

#define MAX(a,b) a > b ? a : b
#define MIN(a,b) a < b ? a : b

static BOOL AndRectRect(struct _Rectangle * rect1, struct _Rectangle * rect2,
    struct _Rectangle * intersect)
{
    intersect->MinX = MAX(rect1->MinX, rect2->MinX);
    intersect->MinY = MAX(rect1->MinY, rect2->MinY);
    intersect->MaxX = MIN(rect1->MaxX, rect2->MaxX);
    intersect->MaxY = MIN(rect1->MaxY, rect2->MaxY);
    
    if ((intersect->MinX > intersect->MaxX) ||
        (intersect->MinY > intersect->MaxY))
        return FALSE;
    else
        return TRUE;
}

static struct StackBitMapNode * HIDDCompositorIsBitMapOnStack(struct HIDDCompositorData * compdata, OOP_Object * bm)
{
    struct StackBitMapNode * n = NULL;
    
    ForeachNode(&compdata->bitmapstack, n)
    {
        if (n->bm == bm)
            return n;
    }

    return NULL;
}

static struct StackBitMapNode * HIDDCompositorCanUseFramebuffer(struct HIDDCompositorData * compdata)
{
    struct StackBitMapNode * n = NULL;
    struct StackBitMapNode * frn = NULL;
         
    ForeachNode(&compdata->bitmapstack, n)
    {
                if( n->isscreenvisible )
                {
                        if( n->screenvisiblerect.MinX <= 0 &&
                                n->screenvisiblerect.MinY <= 0 &&
                                n->screenvisiblerect.MaxX >= compdata->screenrect.MaxX &&
                n->screenvisiblerect.MaxY >= compdata->screenrect.MaxY )
                        {
                                frn = n;
                        }
                        else return NULL;
                }
    }
        D(bug("[Compositor] HIDDCompositorCanUseFramebuffer bm %x\n",frn->bm));
    return frn;
}

static VOID HIDDCompositorRecalculateVisibleRects(struct HIDDCompositorData * compdata)
{
    ULONG lastscreenvisibleline = compdata->screenrect.MaxY + 1;
    struct StackBitMapNode * n = NULL;
        
    ForeachNode(&compdata->bitmapstack, n)
    {
        /*  Stack bitmap bounding boxes equal screen bounding box taking into
            account topedge */
        IPTR topedge;
        struct _Rectangle tmprect;
        
        OOP_GetAttr(n->bm, aHidd_BitMap_TopEdge, &topedge);
        /* Copy screen rect */
        tmprect = compdata->screenrect;
        /* Set bottom and top values */
        tmprect.MinY = topedge;
        tmprect.MaxY = lastscreenvisibleline - 1;
        /* Intersect both to make sure values are withint screen limit */
        if (AndRectRect(&tmprect, &compdata->screenrect, &n->screenvisiblerect))
        {
            lastscreenvisibleline = n->screenvisiblerect.MinY;
            n->isscreenvisible = TRUE;
        }
        else
            n->isscreenvisible = FALSE;

        D(bug("[Compositor] Bitmap %x, visible %d, (%d, %d) , (%d, %d)\n",
            n->bm, n->isscreenvisible,
            n->screenvisiblerect.MinX, n->screenvisiblerect.MinY,
            n->screenvisiblerect.MaxX, n->screenvisiblerect.MaxY));
                        
    }
        
}

static VOID HIDDCompositorRecalculateDisplayedWidthHeight(struct HIDDCompositorData * compdata)
{
    struct StackBitMapNode * n = NULL;
    IPTR displayedwidth, displayedheight;

    OOP_GetAttr(compdata->screenbitmap, aHidd_BitMap_Width, &displayedwidth);
    OOP_GetAttr(compdata->screenbitmap, aHidd_BitMap_Height, &displayedheight);
    
    ForeachNode(&compdata->bitmapstack, n)
    {
        n->displayedwidth   = displayedwidth;
        n->displayedheight  = displayedheight;
    }
}

static BOOL HIDDCompositorTopBitMapChanged(struct HIDDCompositorData * compdata, OOP_Object * bm)
{
    /*
        Desctiption:
        a) take incomming top bitmap
        b) read its mode and sizes,
        c) create a mirroring bitmap that fits the mode
        d) switch mode (driver dependandant)
    */
    OOP_Object * sync = NULL;
    OOP_Object * pf = NULL;
    OOP_Object * fbbitmap = NULL;
    IPTR modeid, hdisp, vdisp, e, depth;
    struct TagItem bmtags[5];

    OOP_GetAttr(bm, aHidd_BitMap_GfxHidd, &e);

    /* Sanity check */
    if (compdata->gfx != (OOP_Object *)e)
    {
        /* Provided top bitmap is not using the same driver as compositor. Fail. */
        D(bug("[Compositor] GfxHidd different than one used by compositor\n"));
        return FALSE;
    }

    /* Read display mode properties */
    OOP_GetAttr(bm, aHidd_BitMap_ModeID, &modeid);
    if (modeid == vHidd_ModeID_Invalid)
    {
        D(bug("[Compositor] Invalid ModeID\n"));
        return FALSE;
    }

    /* If the mode is already visible do nothing */
    if (modeid == compdata->screenmodeid)
        return TRUE;
    
    /* Get width and height of mode */
    struct pHidd_Gfx_GetMode __getmodemsg =
    {
        .modeID     = modeid,
        .syncPtr    = &sync,
        .pixFmtPtr  = &pf,
    }, *getmodemsg = &__getmodemsg;

    getmodemsg->mID = OOP_GetMethodID(IID_Hidd_Gfx, moHidd_Gfx_GetMode);
    OOP_DoMethod(compdata->gfx, (OOP_Msg)getmodemsg);

    OOP_GetAttr(sync, aHidd_Sync_HDisp, &hdisp);
    OOP_GetAttr(sync, aHidd_Sync_VDisp, &vdisp);
    OOP_GetAttr(pf, aHidd_PixFmt_Depth, &depth);

    /* Create a new bitmap that will be used for framebuffer */
    bmtags[0].ti_Tag = aHidd_BitMap_Width;          bmtags[0].ti_Data = hdisp;
    bmtags[1].ti_Tag = aHidd_BitMap_Height;         bmtags[1].ti_Data = vdisp;
    bmtags[2].ti_Tag = aHidd_BitMap_Displayable;    bmtags[2].ti_Data = TRUE;
    bmtags[3].ti_Tag = aHidd_BitMap_ModeID;         bmtags[3].ti_Data = modeid;
    bmtags[4].ti_Tag = TAG_DONE;                    bmtags[4].ti_Data = TAG_DONE;
    
    fbbitmap = HIDD_Gfx_CreateObject(compdata->gfx, SD(OOP_OCLASS(compdata->gfx))->basebm, bmtags);
    if (fbbitmap)
    {
        BOOL ret = HIDD_INTELG45_SwitchToVideoMode(fbbitmap);
        if (ret)
        {

            struct TagItem gctags[] =
            {
                { aHidd_GC_Foreground, (HIDDT_Pixel)0x99999999 },
                { TAG_DONE, TAG_DONE }
            };


            /* Dispose the previous screenbitmap */
            if (compdata->screenbitmap)
                OOP_DisposeObject(compdata->screenbitmap);

            /* Store bitmap/mode information */
            compdata->screenmodeid    = modeid;
            compdata->screenbitmap    = fbbitmap;
            compdata->screenrect.MinX = 0;
            compdata->screenrect.MinY = 0;
            compdata->screenrect.MaxX = hdisp - 1;
            compdata->screenrect.MaxY = vdisp - 1;
            compdata->directbitmap = NULL;
                        
            /* Get gray foregound */
            if (depth < 24) gctags[0].ti_Data = (HIDDT_Pixel)0x9492;
            OOP_SetAttrs(compdata->gc, gctags);
            
            return TRUE;
        }
        else
        {
            /* Dispose fbbitmap */
            OOP_DisposeObject(fbbitmap);
            return FALSE;
        }
    }

    return FALSE;
}

static BOOL HIDDCompositorCanCompositeWithScreenBitMap(struct HIDDCompositorData * compdata, OOP_Object * bm)
{
    OOP_Object * screenbm = compdata->screenbitmap;
    IPTR screenbmwidth, screenbmheight, screenbmstdpixfmt;
    IPTR bmgfx, bmmodeid, bmwidth, bmheight, bmstdpixfmt;
    IPTR pf;
    
    /* HINT: both bitmaps can have different classes */
    OOP_GetAttr(screenbm, aHidd_BitMap_Width, &screenbmwidth);
    OOP_GetAttr(screenbm, aHidd_BitMap_Height, &screenbmheight);
    OOP_GetAttr(screenbm, aHidd_BitMap_PixFmt, &pf);
    OOP_GetAttr((OOP_Object*)pf, aHidd_PixFmt_StdPixFmt, &screenbmstdpixfmt);

    OOP_GetAttr(bm, aHidd_BitMap_GfxHidd, &bmgfx);
    OOP_GetAttr(bm, aHidd_BitMap_ModeID, &bmmodeid);
    OOP_GetAttr(bm, aHidd_BitMap_Width, &bmwidth);
    OOP_GetAttr(bm, aHidd_BitMap_Height, &bmheight);
    OOP_GetAttr(bm, aHidd_BitMap_PixFmt, &pf);
    OOP_GetAttr((OOP_Object*)pf, aHidd_PixFmt_StdPixFmt, &bmstdpixfmt);

    /* If bm uses different instances of gfx hidd than screenbm(=composing), they cannot be composited */
    if (compdata->gfx != (OOP_Object *)bmgfx)
        return FALSE;
    
    /* If bitmaps have the same modeid, they can be composited */
    if (compdata->screenmodeid == bmmodeid)
        return TRUE;

    /* If bitmaps have different pixel formats, they cannot be composited */
    /* actually they can, if hw conversion is supported. */
    if ( ( screenbmstdpixfmt != bmstdpixfmt ) && !copybox3d_supported() )
        return FALSE;

    return TRUE;
}

static VOID HIDDCompositorRedrawBitmap(struct HIDDCompositorData * compdata,
    OOP_Object * bm, WORD x, WORD y, WORD width, WORD height, BOOL clean)
{
    struct StackBitMapNode * n = NULL;
    
    /* Check if passed bitmap is in stack, ignore if not */
    if ((n = HIDDCompositorIsBitMapOnStack(compdata, bm)) == NULL)
        return;

    if (!n->isscreenvisible)
        return;

    if (compdata->screenbitmap)
    {
        IPTR leftedge, topedge;
        struct _Rectangle srcrect;
        struct _Rectangle srcindstrect;
        struct _Rectangle dstandvisrect;

        OOP_GetAttr(bm, aHidd_BitMap_LeftEdge, &leftedge);
        OOP_GetAttr(bm, aHidd_BitMap_TopEdge, &topedge);
        
        /* Rectangle in source bitmap coord system */
        srcrect.MinX = x;
        srcrect.MinY = y;
        srcrect.MaxX = x + width - 1;
        srcrect.MaxY = y + height - 1;
        
        /* Source bitmap rectangle in destination (screen) coord system */
        srcindstrect.MinX = srcrect.MinX + leftedge;
        srcindstrect.MaxX = srcrect.MaxX + leftedge;
        srcindstrect.MinY = srcrect.MinY + topedge;
        srcindstrect.MaxY = srcrect.MaxY + topedge;
        
        /* Find intersection of bitmap visible screen rect and srcindst rect */
        if (AndRectRect(&srcindstrect, &n->screenvisiblerect, &dstandvisrect))
        {
            /* Intersection is valid. Blit. */

            HIDD_Gfx_CopyBox(
                compdata->gfx,
                bm,
                /* Transform back to source bitmap coord system */
                dstandvisrect.MinX - leftedge, dstandvisrect.MinY - topedge,
                compdata->screenbitmap,
                dstandvisrect.MinX, dstandvisrect.MinY,
                dstandvisrect.MaxX - dstandvisrect.MinX + 1,
                dstandvisrect.MaxY - dstandvisrect.MinY + 1,
                compdata->gc);
                
            if( clean && dstandvisrect.MaxX+1 <  n->screenvisiblerect.MaxX )
            {
                /* Clean right side.*/
                D(bug("[Compositor] clean right %d,%d-%d,%d\n",dstandvisrect.MaxX+1, dstandvisrect.MinY,
                    n->screenvisiblerect.MaxX, dstandvisrect.MaxY));
                HIDD_BM_FillRect(compdata->screenbitmap, compdata->gc,
                    dstandvisrect.MaxX+1, dstandvisrect.MinY-1,
                    n->screenvisiblerect.MaxX, dstandvisrect.MaxY);
            }
            
            if( clean && dstandvisrect.MaxY+1 <  n->screenvisiblerect.MaxY )
            {
                /* Clean bottom.*/
                D(bug("[Compositor] clean bottom %d,%d-%d,%d\n",
                    dstandvisrect.MinX, dstandvisrect.MaxY+1,
                    n->screenvisiblerect.MaxX, n->screenvisiblerect.MaxY));
                HIDD_BM_FillRect(compdata->screenbitmap, compdata->gc,
                    dstandvisrect.MinX, dstandvisrect.MaxY+1,
                    n->screenvisiblerect.MaxX, n->screenvisiblerect.MaxY);
            }
            
        }
    }
}

static VOID HIDDCompositorRedrawVisibleScreen(struct HIDDCompositorData * compdata)
{
    struct StackBitMapNode * n = NULL;
    ULONG lastscreenvisibleline = compdata->screenrect.MaxY;
    
    /* Recalculate visible rects per screen */
    HIDDCompositorRecalculateVisibleRects(compdata);

        /* use direct framebuffer if it is possible */
        struct StackBitMapNode  *fbn;
        if( ( fbn =  HIDDCompositorCanUseFramebuffer(compdata) ) )
        {
                if( HIDD_INTELG45_SetFramebuffer( fbn->bm ) ){
                        D(bug("[Compositor] directbitmap used, bitmap %x\n",fbn->bm););
                        compdata->directbitmap = fbn->bm;
                }
        }
        else
        {
                if( compdata->directbitmap ){
                        compdata->directbitmap = NULL;
                        D(bug("[Compositor] directbitmap not used, set screenbitmap %x\n",compdata->screenbitmap););
                        HIDD_INTELG45_SetFramebuffer( compdata->screenbitmap );
                }
        }

    /* Refresh all bitmaps on stack */
    ForeachNode(&compdata->bitmapstack, n)
    {
        if (n->isscreenvisible)
        {
            IPTR width, height;

            OOP_GetAttr(n->bm, aHidd_BitMap_Width, &width);
            OOP_GetAttr(n->bm, aHidd_BitMap_Height, &height);

            HIDDCompositorRedrawBitmap(compdata, n->bm, 0, 0, width, height, TRUE);
            if (lastscreenvisibleline > n->screenvisiblerect.MinY)
                lastscreenvisibleline = n->screenvisiblerect.MinY;
        }
    }

    /* Clean up area revealed by drag */
    /* TODO: Find all areas which might have been releaved, not only top -
       This will happen when there are bitmaps of different sizes composited */
    if (lastscreenvisibleline > 0)
    {
        IPTR viswidth;

        OOP_GetAttr(compdata->screenbitmap, aHidd_BitMap_Width, &viswidth);
        D(bug("[Compositor] Clean upper area %d,%d-%d,%d\n",0, 0, viswidth - 1, lastscreenvisibleline - 1));
        HIDD_BM_FillRect(compdata->screenbitmap,
            compdata->gc, 0, 0, viswidth - 1, lastscreenvisibleline - 1);
    }
}

static VOID HIDDCompositorPurgeBitMapStack(struct HIDDCompositorData * compdata)
{
    struct StackBitMapNode * curr, * next;

    ForeachNodeSafe(&compdata->bitmapstack, curr, next)
    {
        Remove((struct Node *)curr);
        FreeMem(curr, sizeof(struct StackBitMapNode));
    }
    
    NEWLIST(&compdata->bitmapstack);
}




/* PUBLIC METHODS */
OOP_Object *METHOD(Compositor, Root, New)
{
    o = (OOP_Object *)OOP_DoSuperMethod(cl, o, (OOP_Msg) msg);
    D(bug("[Compositor] New\n"));
    if(o)
    {
        struct HIDDCompositorData * compdata = OOP_INST_DATA(cl, o);

        NEWLIST(&compdata->bitmapstack);
        compdata->screenbitmap  = NULL;
        compdata->directbitmap  = NULL;
        compdata->screenmodeid  = vHidd_ModeID_Invalid;
        InitSemaphore(&compdata->semaphore);
                
        compdata->gfx = (OOP_Object *)GetTagData(aHidd_Compositor_GfxHidd, 0, msg->attrList);
        
        if (compdata->gfx != NULL)
        {
            /* Create GC object that will be used for drawing operations */
            compdata->gc = HIDD_Gfx_CreateObject(compdata->gfx, SD(OOP_OCLASS(compdata->gfx))->basegc, NULL);
        }
        
        if ((compdata->gfx == NULL) || (compdata->gc == NULL))
        {
            /* Creation failed */
            OOP_MethodID disposemid;
            disposemid = OOP_GetMethodID(IID_Root, moRoot_Dispose);
            OOP_CoerceMethod(cl, o, (OOP_Msg)&disposemid);
            o = NULL;
        }
    }

    return o;
}

VOID METHOD(Compositor, Hidd_Compositor, BitMapStackChanged)
{
    struct HIDD_ViewPortData * vpdata;
    struct HIDDCompositorData * compdata = OOP_INST_DATA(cl, o);

    D(bug("[Compositor] BitMapStackChanged\n"));

    LOCK_COMPOSITING_WRITE
        
    /* Free all items which are already on the list */
    HIDDCompositorPurgeBitMapStack(compdata);
    
    
    if (!msg->data)
    {
        UNLOCK_COMPOSITING
        return; /* TODO: BLANK SCREEN */
    }
    
    /* Switch mode if needed */
    if (!HIDDCompositorTopBitMapChanged(compdata, msg->data->Bitmap))
    {
        /* Something bad happened. Yes, bitmap stack is already erased - that's ok */
        D(bug("[Compositor] Failed to change top bitmap\n"));
        UNLOCK_COMPOSITING
        return;
    }
    
    /* Copy bitmaps pointers to our stack */
    for (vpdata = msg->data; vpdata; vpdata = vpdata->Next)
    {
        /* Check if the passed bitmap can be composited together with screen
           bitmap */
        if (HIDDCompositorCanCompositeWithScreenBitMap(compdata, vpdata->Bitmap))
        {
            struct StackBitMapNode * n = AllocMem(sizeof(struct StackBitMapNode), MEMF_ANY | MEMF_CLEAR);
            n->bm               = vpdata->Bitmap;
            n->isscreenvisible  = FALSE;
            n->displayedwidth   = 0;
            n->displayedheight  = 0;
            AddTail(&compdata->bitmapstack, (struct Node *)n);
        }
    }

    /* Set displayedwidth/displayedheight on all screen bitmaps */
    HIDDCompositorRecalculateDisplayedWidthHeight(compdata);
    
    /* Redraw bitmap stack */
    HIDDCompositorRedrawVisibleScreen(compdata);
    
    UNLOCK_COMPOSITING
}

VOID METHOD(Compositor, Hidd_Compositor, BitMapRectChanged)
{
    struct HIDDCompositorData * compdata = OOP_INST_DATA(cl, o);
    //D(bug("[Compositor] BitMapRectChanged\n"));
        if( ! compdata->directbitmap ){
                LOCK_COMPOSITING_READ
                HIDDCompositorRedrawBitmap(compdata, msg->bm, msg->x, msg->y, msg->width, msg->height, FALSE);
                UNLOCK_COMPOSITING
        }
}

VOID METHOD(Compositor, Hidd_Compositor, BitMapPositionChanged)
{
    struct HIDDCompositorData * compdata = OOP_INST_DATA(cl, o);
    D(bug("[Compositor] BitMapPositionChanged\n"));
    LOCK_COMPOSITING_READ

    /* Check is passed bitmap is in stack, ignore if not */
    if (HIDDCompositorIsBitMapOnStack(compdata, msg->bm) != NULL)
    {
        /* Redraw bitmap stack */
        HIDDCompositorRedrawVisibleScreen(compdata);
    }
    
    UNLOCK_COMPOSITING
}

VOID METHOD(Compositor, Hidd_Compositor, ValidateBitMapPositionChange)
{
    struct HIDDCompositorData * compdata = OOP_INST_DATA(cl, o);
    struct StackBitMapNode * n = NULL;
      D(bug("[Compositor] ValidateBitMapPositionChange %d,%d\n",
                 *msg->newxoffset,*msg->newyoffset));

    LOCK_COMPOSITING_READ
    
    /* Check if passed bitmap is in stack, ignore if not */
    if ((n = HIDDCompositorIsBitMapOnStack(compdata, msg->bm)) != NULL)
    {
        IPTR width, height;
        LONG limit;
        
        OOP_GetAttr(msg->bm, aHidd_BitMap_Width, &width);
        OOP_GetAttr(msg->bm, aHidd_BitMap_Height, &height);

        /* Check x position */
        limit = n->displayedwidth < width ? n->displayedwidth - width : 0;
        if (*(msg->newxoffset) > 0)
            *(msg->newxoffset) = 0;

        if (*(msg->newxoffset) < limit)
            *(msg->newxoffset) = limit;

        /* Check y position */
        limit = n->displayedheight < height ? n->displayedheight - height : 0;
        if (*(msg->newyoffset) > n->displayedheight - 15) /* Limit for drag */
            *(msg->newyoffset) = n->displayedheight - 15;

        if (*(msg->newyoffset) < limit) /* Limit for scroll */
            *(msg->newyoffset) = limit;
    }
    
    UNLOCK_COMPOSITING
}


static const struct OOP_MethodDescr Compositor_Root_descr[] =
{
    {(OOP_MethodFunc)Compositor__Root__New, moRoot_New},
    {NULL, 0}
};
#define NUM_Compositor_Root_METHODS 1

static const struct OOP_MethodDescr Compositor_Hidd_Compositor_descr[] =
{
    {(OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapStackChanged, moHidd_Compositor_BitMapStackChanged},
    {(OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapRectChanged, moHidd_Compositor_BitMapRectChanged},
    {(OOP_MethodFunc)Compositor__Hidd_Compositor__BitMapPositionChanged, moHidd_Compositor_BitMapPositionChanged},
    {(OOP_MethodFunc)Compositor__Hidd_Compositor__ValidateBitMapPositionChange, moHidd_Compositor_ValidateBitMapPositionChange},
    {NULL, 0}
};
#define NUM_Compositor_Hidd_Compositor_METHODS 4

const struct OOP_InterfaceDescr Compositor_ifdescr[] =
{
    {Compositor_Root_descr, IID_Root, NUM_Compositor_Root_METHODS},
    {Compositor_Hidd_Compositor_descr, IID_Hidd_Compositor, NUM_Compositor_Hidd_Compositor_METHODS},
    {NULL, NULL}
};

