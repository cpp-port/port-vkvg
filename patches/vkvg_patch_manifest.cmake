set(VKVG_PATCHED_RELATIVE_SOURCES
  src/vkvg_context_internal.c
  src/vkvg_device.c
  src/vkvg_device_internal.c)

set(VKVG_PATCH_FILES
  0001-clear-fullscreen-bit-before-indexed-draw.patch
  0002-fix-non-zero-fill-winding.patch
  0003-honor-requested-sample-count.patch
  0004-strengthen-render-pass-handoffs.patch)
