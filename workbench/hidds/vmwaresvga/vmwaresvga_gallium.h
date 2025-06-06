#ifndef _VMWARESVGA_INTERN_H
#define _VMWARESVGA_INTERN_H

/*
    Copyright 2010-2025, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <hidd/gallium.h>

#include "vmwaresvga_hardware.h"

#include "svga/svga_winsys.h"
#include "svga/svga_public.h"

#include "util/u_memory.h"
#include "pipebuffer/pb_buffer.h"

#include "svga3d_caps.h"
#include "svga_cmd.h"

#define CLID_Hidd_Gallium_VMWareSVGA  "hidd.gallium.vmwaresvga"

#define VMW_MAX_DEFAULT_TEXTURE_SIZE   (128 * 1024 * 1024)

#define VMW_COMMAND_SIZE (64*1024)
#define VMW_SURFACE_RELOCS (1024)
#define VMW_SHADER_RELOCS (1024)
#define VMW_REGION_RELOCS (512)

struct VMWareSVGAPBBuf
{
    struct Node                         bnode;
    ULONG                               magic;
    APTR                                map;
    ULONG                               size;
    void                                *ptr;
    ULONG                               stride;
    APTR                                allocated_map;
    ULONG                               allocated_size;
    ULONG                               status_index;
    ULONG                               flush_num;
};

struct VMWareSVGA3DCap {
    BOOL                        has_cap;
    SVGA3dDevCapResult          result;
};

struct HIDDGalliumVMWareSVGAData
{
    struct svga_winsys_screen   wssbase;
    OOP_Object                  *wsgo;
    struct HWData               *hwdata;

    ULONG                       ctxcnt;
    ULONG                       srfcnt;

    ULONG                       use_gbobjects;

    ULONG                       size_3dcaps;
    struct VMWareSVGA3DCap      *cap_3d;
};

struct HIDDGalliumVMWareSVGACtx
{
    struct svga_winsys_context wscbase;
    struct svga_winsys_screen *wscsws;

    volatile struct VMWareSVGAFIFO *command;

    struct {
#if (0)
        struct vmw_ctx_validate_item items[VMW_SURFACE_RELOCS];
#endif
        ULONG size;
        ULONG used;
        ULONG staged;
        ULONG reserved;
    } surface;

    struct {
#if (0)
        struct vmw_buffer_relocation relocs[VMW_REGION_RELOCS];
#endif
        ULONG size;
        ULONG used;
        ULONG staged;
        ULONG reserved;
    } region;

    struct {
#if (0)
        struct vmw_ctx_validate_item items[VMW_SHADER_RELOCS];
#endif
        ULONG size;
        ULONG used;
        ULONG staged;
        ULONG reserved;
    } shader;
};

struct HIDDGalliumVMWareSVGASurf
{
    struct svga_winsys_buffer   *surfbuf;
    struct pipe_reference       refcnt;
    ULONG                       sid;                                    // Hardware surface ID
};

struct HIDDGalliumVMWareSVGAShader
{
    struct svga_winsys_buffer   *shaderbuf;
    struct pipe_reference       refcnt;
};

/* winsys functions */
static inline struct svga_winsys_surface *VMWareSVGA_WSSurf_WinSysSurfFromHiddSurf(struct HIDDGalliumVMWareSVGASurf *surf)
{
    return (struct svga_winsys_surface *)surf;
}

static inline struct HIDDGalliumVMWareSVGASurf *VMWareSVGA_WSSurf_HiddSurfFromWinSysSurf(struct svga_winsys_surface *surf)
{
    return (struct HIDDGalliumVMWareSVGASurf *)surf;
}

static inline struct svga_winsys_gb_shader *VMWareSVGA_WSSurf_WinsysShaderHiddShader(struct HIDDGalliumVMWareSVGAShader *shader)
{
   return (struct svga_winsys_gb_shader *)shader;
}

static inline struct HIDDGalliumVMWareSVGAData *VMWareSVGA_WSScr_HiddDataFromWinSys(struct svga_winsys_screen *sws)
{
    return (struct HIDDGalliumVMWareSVGAData *)sws;
}

void VMWareSVGA_WSScr_WinSysInit(struct HIDDGalliumVMWareSVGAData *);
void VMWareSVGA_WSCtx_WinSysInit(struct HIDDGalliumVMWareSVGAData *, struct HIDDGalliumVMWareSVGACtx *);

void *VMWareSVGA_WSSurf_SurfaceMap(struct svga_winsys_context *swc, struct svga_winsys_surface *srf, unsigned flags, boolean *retry, boolean *rebind);
void VMWareSVGA_WSSurf_SurfaceUnMap(struct svga_winsys_context *swc, struct svga_winsys_surface *srf, boolean *rebind);
void VMWareSVGA_WSSurf_SurfaceReference(struct HIDDGalliumVMWareSVGASurf **, struct HIDDGalliumVMWareSVGASurf *);

#endif
