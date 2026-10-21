# Vulkan C headers

Unchanged C declarations from KhronosGroup/Vulkan-Headers, tag `v1.3.290`,
commit `b379292b2ab6df5771ba9870d53cf8b2c9295daf`:
https://github.com/KhronosGroup/Vulkan-Headers/tree/b379292b2ab6df5771ba9870d53cf8b2c9295daf

Archive SHA256:
`59154aee362147f1f234dff71f1d8c8572cd522596496c07a5cc25cc446256a5`

`SHA256SUMS` records every retained upstream file. Included are
`vulkan_core.h`, `vk_platform.h`, the video declarations that core includes,
and the upstream license files. The C++ wrappers, platform-specific
declarations, generators and registry are not needed by this initial target.
The headers carry Apache-2.0 OR MIT terms; retain the notices and licenses.

Used by the native Vulkan backend and its tests under
`port/linux/src/vulkan/`, including the optional Windows/Linux presentation
test in the game executable. They do not supply an implementation or require
an installed SDK. OpenGL/GLES remains the normal rendering path. The backend
loads the operating system's Vulkan loader at runtime. Header version is
independent of the initial runtime baseline, Vulkan 1.1.
