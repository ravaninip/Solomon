#pragma once

// Force-included (MSVC only) into the vendored Mutable Instruments code and the file that wraps it.
// The upstream headers use GCC attributes such as __attribute__((always_inline)), which MSVC does
// not understand. They are optimisation hints only, so dropping them changes no behaviour.
#if defined(_MSC_VER) && ! defined(__clang__)
 #define __attribute__(x)
#endif
