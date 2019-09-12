/******************************************************************************
** Copyright (c) 2016-2019, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Hans Pabst (Intel Corp.)
******************************************************************************/
#include <libxsmm_intrinsics_x86.h>
#include <libxsmm_generator.h>

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#include <limits.h>
#include <stdio.h>
#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(pop)
#endif

#if defined(LIBXSMM_PLATFORM_SUPPORTED)
/* XGETBV: receive results (EAX, EDX) for eXtended Control Register (XCR). */
/* CPUID, receive results (EAX, EBX, ECX, EDX) for requested FUNCTION/SUBFN. */
  #if defined(_MSC_VER) /*defined(_WIN32) && !defined(__GNUC__)*/
#   define LIBXSMM_XGETBV(XCR, EAX, EDX) { \
      unsigned long long libxsmm_xgetbv_ = _xgetbv(XCR); \
      EAX = (int)libxsmm_xgetbv_; \
      EDX = (int)(libxsmm_xgetbv_ >> 32); \
    }
#   define LIBXSMM_CPUID_X86(FUNCTION, SUBFN, EAX, EBX, ECX, EDX) { \
      int libxsmm_cpuid_x86_[/*4*/] = { 0, 0, 0, 0 }; \
      __cpuidex(libxsmm_cpuid_x86_, FUNCTION, SUBFN); \
      EAX = (unsigned int)libxsmm_cpuid_x86_[0]; \
      EBX = (unsigned int)libxsmm_cpuid_x86_[1]; \
      ECX = (unsigned int)libxsmm_cpuid_x86_[2]; \
      EDX = (unsigned int)libxsmm_cpuid_x86_[3]; \
    }
# elif defined(__GNUC__) || !defined(_CRAYC)
#   if (64 > (LIBXSMM_BITS))
      LIBXSMM_EXTERN LIBXSMM_RETARGETABLE int __get_cpuid(unsigned int, unsigned int*, unsigned int*, unsigned int*, unsigned int*);
#     define LIBXSMM_XGETBV(XCR, EAX, EDX) EAX = (EDX) = 0xFFFFFFFF
#     define LIBXSMM_CPUID_X86(FUNCTION, SUBFN, EAX, EBX, ECX, EDX) \
        EAX = (EBX) = (EDX) = 0; ECX = (SUBFN); \
        __get_cpuid(FUNCTION, &(EAX), &(EBX), &(ECX), &(EDX))
#   else /* 64-bit */
#     define LIBXSMM_XGETBV(XCR, EAX, EDX) __asm__ __volatile__( \
        ".byte 0x0f, 0x01, 0xd0" /*xgetbv*/ : "=a"(EAX), "=d"(EDX) : "c"(XCR) \
      )
#     define LIBXSMM_CPUID_X86(FUNCTION, SUBFN, EAX, EBX, ECX, EDX) \
        __asm__ __volatile__ (".byte 0x0f, 0xa2" /*cpuid*/ \
        : "=a"(EAX), "=b"(EBX), "=c"(ECX), "=d"(EDX) \
        : "a"(FUNCTION), "b"(0), "c"(SUBFN), "d"(0) \
      )
#   endif
# else /* legacy Cray Compiler */
#   define LIBXSMM_XGETBV(XCR, EAX, EDX) EAX = (EDX) = 0
#   define LIBXSMM_CPUID_X86(FUNCTION, SUBFN, EAX, EBX, ECX, EDX) EAX = (EBX) = (ECX) = (EDX) = 0
# endif
#endif

#define LIBXSMM_CPUID_CHECK(VALUE, CHECK) ((CHECK) == ((CHECK) & (VALUE)))


LIBXSMM_API int libxsmm_cpuid_x86(void)
{
#if defined(LIBXSMM_INTRINSICS_DEBUG)
  int result = LIBXSMM_X86_GENERIC;
#else
  int result = LIBXSMM_STATIC_TARGET_ARCH;
#endif
#if defined(LIBXSMM_PLATFORM_SUPPORTED)
  unsigned int eax, ebx, ecx, edx;
  LIBXSMM_CPUID_X86(0, 0/*ecx*/, eax, ebx, ecx, edx);
  if (1 <= eax) { /* CPUID max. leaf */
    int feature_cpu = result, feature_os = result;
    unsigned int maxleaf = eax, ecx2, ecx3;
    LIBXSMM_CPUID_X86(1, 0/*ecx*/, eax, ebx, ecx, edx);
    /* Check for CRC32 (this is not a proper test for SSE 4.2 as a whole!) */
    if (LIBXSMM_CPUID_CHECK(ecx, 0x00100000)) {
      if (LIBXSMM_CPUID_CHECK(ecx, 0x10000000)) { /* AVX(0x10000000) */
        if (LIBXSMM_CPUID_CHECK(ecx, 0x00001000)) { /* FMA(0x00001000) */
          LIBXSMM_CPUID_X86(7, 0/*ecx*/, eax, ebx, ecx2, edx);
          /* AVX512F(0x00010000), AVX512CD(0x10000000) */
          if (LIBXSMM_CPUID_CHECK(ebx, 0x10010000)) { /* Common */
            /* AVX512DQ(0x00020000), AVX512BW(0x40000000), AVX512VL(0x80000000) */
            if (LIBXSMM_CPUID_CHECK(ebx, 0xC0020000)) { /* AVX512-Core */
              if (LIBXSMM_CPUID_CHECK(ecx2, 0x00000800)) { /* VNNI */
                LIBXSMM_CPUID_X86(7, 1/*ecx*/, eax, ebx, ecx3, edx);
                if (LIBXSMM_CPUID_CHECK(eax, 0x00000020)) { /* BF16 */
                  feature_cpu = LIBXSMM_X86_AVX512_CPX;
                }
                else feature_cpu = LIBXSMM_X86_AVX512_CLX; /* CLX */
              }
              else feature_cpu = LIBXSMM_X86_AVX512_CORE; /* SKX */
            }
            /* AVX512PF(0x04000000), AVX512ER(0x08000000) */
            else if (LIBXSMM_CPUID_CHECK(ebx, 0x0C000000)) { /* AVX512-MIC */
              if (LIBXSMM_CPUID_CHECK(edx, 0x0000000C)) { /* KNM */
                feature_cpu = LIBXSMM_X86_AVX512_KNM;
              }
              else feature_cpu = LIBXSMM_X86_AVX512_MIC; /* KNL */
            }
            else feature_cpu = LIBXSMM_X86_AVX512; /* AVX512-Common */
          }
          else feature_cpu = LIBXSMM_X86_AVX2;
        }
        else feature_cpu = LIBXSMM_X86_AVX;
      }
      else feature_cpu = LIBXSMM_X86_SSE4;
    }
    /* XSAVE/XGETBV(0x04000000), OSXSAVE(0x08000000) */
    if (LIBXSMM_X86_SSE3 <= feature_cpu && LIBXSMM_CPUID_CHECK(ecx, 0x0C000000)) { /* OS SSE support */
      feature_os = LIBXSMM_X86_SSE4;
      if (LIBXSMM_X86_AVX <= feature_cpu) {
        LIBXSMM_XGETBV(0, eax, edx);
        if (LIBXSMM_CPUID_CHECK(eax, 0x00000006)) { /* OS XSAVE 256-bit */
          feature_os = LIBXSMM_X86_AVX2;
          if (LIBXSMM_X86_AVX512 <= feature_cpu && 7 <= maxleaf
           && LIBXSMM_CPUID_CHECK(eax, 0x000000E0)) /* OS XSAVE 512-bit */
          {
            feature_os = INT_MAX; /* unlimited */
          }
        }
      }
    }
    else feature_os = LIBXSMM_X86_GENERIC;
#if 0
    result = LIBXSMM_MIN(feature_os, feature_cpu);
#else
    if (LIBXSMM_STATIC_TARGET_ARCH < feature_cpu && feature_os < feature_cpu
      && 0 != libxsmm_verbosity) /* library code is expected to be mute */
    {
      fprintf(stderr, "LIBXSMM WARNING: detected CPU features are not permitted by the OS!\n");
    }
    result = feature_cpu;
#endif
  }
#endif
#if defined(LIBXSMM_INTRINSICS_DEBUG)
  return result;
#else /* check if procedure obviously failed to detect the highest available instruction set extension */
  LIBXSMM_ASSERT(LIBXSMM_STATIC_TARGET_ARCH <= result);
  return LIBXSMM_MAX(result, LIBXSMM_STATIC_TARGET_ARCH);
#endif
}


LIBXSMM_API int libxsmm_cpuid(void)
{
  return libxsmm_cpuid_x86();
}


LIBXSMM_API const char* libxsmm_cpuid_name(int id)
{
  const char* feature_cpu = NULL;
  switch (id) {
    case LIBXSMM_X86_AVX512_CPX: {
      feature_cpu = "cpx";
    } break;
    case LIBXSMM_X86_AVX512_CLX: {
      feature_cpu = "clx";
    } break;
    case LIBXSMM_X86_AVX512_CORE: {
      feature_cpu = "skx";
    } break;
    case LIBXSMM_X86_AVX512_KNM: {
      feature_cpu = "knm";
    } break;
    case LIBXSMM_X86_AVX512_MIC: {
      feature_cpu = "knl";
    } break;
    case LIBXSMM_X86_AVX512: {
      /* TODO: rework BE to use target ID instead of set of strings (feature_cpu = "avx3") */
      feature_cpu = "hsw";
    } break;
    case LIBXSMM_X86_AVX2: {
      feature_cpu = "hsw";
    } break;
    case LIBXSMM_X86_AVX: {
      feature_cpu = "snb";
    } break;
    case LIBXSMM_X86_SSE4: {
      /* TODO: rework BE to use target ID instead of set of strings (feature_cpu = "sse4") */
      feature_cpu = "wsm";
    } break;
    case LIBXSMM_X86_SSE3: {
      /* WSM includes SSE4, but BE relies on SSE3 only,
       * hence we enter "wsm" path starting with SSE3.
       */
      feature_cpu = "wsm";
    } break;
    case LIBXSMM_TARGET_ARCH_GENERIC: {
      feature_cpu = "generic";
    } break;
    default: if (LIBXSMM_X86_GENERIC <= id) {
      feature_cpu = "x86";
    }
    else {
      feature_cpu = "unknown";
    }
  }

  LIBXSMM_ASSERT(NULL != feature_cpu);
  return feature_cpu;
}

