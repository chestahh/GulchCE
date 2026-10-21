/* Included only by native platform units; all declarations use the XDK types. */
#ifndef HALO_VULKAN_DISPATCH_H
#define HALO_VULKAN_DISPATCH_H
extern int halo_vulkan_dispatch_active;
HRESULT halo_vulkan_create(D3DPRESENT_PARAMETERS *parameters, D3DDevice **result);
void halo_vulkan_clear(DWORD count, CONST D3DRECT *rectangles, DWORD flags,
	D3DCOLOR color, float depth, DWORD stencil);
void halo_vulkan_present(CONST RECT *source, CONST RECT *destination, void *unused, void *unused2);
ULONG halo_vulkan_release(void);
#endif
