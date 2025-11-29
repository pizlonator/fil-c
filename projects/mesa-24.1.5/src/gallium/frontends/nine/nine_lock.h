/*
 * Copyright 2013 Joakim Sindholt <opensource@zhasha.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef _NINE_LOCK_H_
#define _NINE_LOCK_H_

#include "d3d9.h"
#include "d3dadapter/d3dadapter9.h"

extern IDirect3DAuthenticatedChannel9Vtbl LockAuthenticatedChannel9_vtable;
extern IDirect3DCryptoSession9Vtbl LockCryptoSession9_vtable;
extern IDirect3DCubeTexture9Vtbl LockCubeTexture9_vtable;
extern IDirect3DDevice9Vtbl LockDevice9_vtable;
extern IDirect3DDevice9ExVtbl LockDevice9Ex_vtable;
extern IDirect3DDevice9VideoVtbl LockDevice9Video_vtable;
extern IDirect3DIndexBuffer9Vtbl LockIndexBuffer9_vtable;
extern IDirect3DPixelShader9Vtbl LockPixelShader9_vtable;
extern IDirect3DQuery9Vtbl LockQuery9_vtable;
extern IDirect3DStateBlock9Vtbl LockStateBlock9_vtable;
extern IDirect3DSurface9Vtbl LockSurface9_vtable;
extern IDirect3DSwapChain9Vtbl LockSwapChain9_vtable;
extern IDirect3DSwapChain9ExVtbl LockSwapChain9Ex_vtable;
extern IDirect3DTexture9Vtbl LockTexture9_vtable;
extern IDirect3DVertexBuffer9Vtbl LockVertexBuffer9_vtable;
extern IDirect3DVertexDeclaration9Vtbl LockVertexDeclaration9_vtable;
extern IDirect3DVertexShader9Vtbl LockVertexShader9_vtable;
extern IDirect3DVolume9Vtbl LockVolume9_vtable;
extern IDirect3DVolumeTexture9Vtbl LockVolumeTexture9_vtable;
extern IDirect3DVolumeTexture9Vtbl LockVolumeTexture9_vtable;
extern ID3DAdapter9Vtbl LockAdapter9_vtable;

void NineLockGlobalMutex(void);
void NineUnlockGlobalMutex(void);

#endif /* _NINE_LOCK_H_ */
