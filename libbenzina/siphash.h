/* Include Guard */
#ifndef INCLUDE_BENZINA_SIPHASH_H
#define INCLUDE_BENZINA_SIPHASH_H


/**
 * Includes
 */

#include <stddef.h>
#include <stdint.h>


/* Clean Benzina defines */
#undef BENZINA_ATTRIBUTE_ALWAYSINLINE
#undef BENZINA_ATTRIBUTE_CONST
#undef BENZINA_INLINE
#undef BENZINA_PUBLIC
#undef BENZINA_STATIC
#define BENZINA_ATTRIBUTE_ALWAYSINLINE
#define BENZINA_ATTRIBUTE_CONST
#define BENZINA_INLINE
#define BENZINA_PUBLIC
#define BENZINA_STATIC

/* Extern "C" Guard */
#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief The SipHash Pseudo-Random Function (PRF)
 * 
 * SipHash is a family of pseudo-random functions (PRF) meant for fast hashing
 * of short inputs. Its design goal is to prevent hash-flooding attacks (where
 * a hash table is overwhelmed with collisions) by using a secure 128-bit key,
 * while remaining competitive in speed. It is an ARX (Add-Rotate-Xor)-class
 * algorithm and can thus be efficiently implemented on 64-bit CPUs.
 * 
 * A specific member of the SipHash family, SipHash-c-d, is identified by its
 * parameters c and d, where c is the number of compression rounds per data
 * block, and d is the number of finalization rounds. The authors suggest two
 * defaults:
 * 
 *   - SipHash-2-4 as a fast proposal meant for general-purpose needs.
 *   - SipHash-4-8 as a conservative proposal with greater security margin but
 *                 about half the speed (due to doubling the number of rounds).
 * 
 * The most commonly-used SipHash PRF is SipHash-2-4.
 * 
 * In the present header, we will give the suffix -x to the "generic" SipHash
 * API and no suffix to the SipHash-2-4 specialization.
 * 
 * [1] https://www.aumasson.jp/siphash/siphash.pdf
 */

BENZINA_PUBLIC uint64_t benz_siphash_digest (const void* buf, uint64_t len,
                                             uint64_t    k0,  uint64_t k1);
BENZINA_PUBLIC uint64_t benz_siphash_digestx(const void* buf, uint64_t len,
                                             uint64_t    k0,  uint64_t k1,
                                             unsigned    c,   unsigned d);


/* End Extern "C" and Include Guard */
#ifdef __cplusplus
}
#endif
#endif

