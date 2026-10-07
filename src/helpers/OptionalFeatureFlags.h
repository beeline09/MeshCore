#pragma once

// PlatformIO's `-D NAME=0` still makes NAME visible to `#ifdef NAME`.
// Normalize explicitly disabled optional features so absent and zero-valued
// flags behave identically.
#if defined(WITH_MCOTXT) && !WITH_MCOTXT
  #undef WITH_MCOTXT
#endif

#if defined(WITH_MCMP_DETECT) && !WITH_MCMP_DETECT
  #undef WITH_MCMP_DETECT
#endif

#if defined(WITH_AEIC_DETECT) && !WITH_AEIC_DETECT
  #undef WITH_AEIC_DETECT
#endif

#if defined(WITH_MCOIMG_DETECT) && !WITH_MCOIMG_DETECT
  #undef WITH_MCOIMG_DETECT
#endif
