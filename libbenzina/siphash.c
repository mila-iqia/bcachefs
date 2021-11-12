/* Includes */
#include <string.h>

#include "siphash.h"

/* endian.h */

/* Temporary defines...  */
#define BENZINA_PUBLIC_ENDIANOP        BENZINA_ATTRIBUTE_ALWAYSINLINE BENZINA_PUBLIC BENZINA_INLINE
#define BENZINA_PUBLIC_ENDIANOP_CONST  BENZINA_PUBLIC_ENDIANOP BENZINA_ATTRIBUTE_CONST

#if BENZINA_BIG_ENDIAN
# define BENZ_PICK_BE_LE(A,B) A
#else
# define BENZ_PICK_BE_LE(A,B) B
#endif

BENZINA_PUBLIC_ENDIANOP_CONST uint64_t benz_le64toh(uint64_t x){return BENZ_PICK_BE_LE(benz_bswap64(x), x);}

BENZINA_PUBLIC_ENDIANOP       uint64_t benz_getle64(const void* p, int64_t off){
    uint64_t x;
    memcpy(&x, (char*)p+off, sizeof(x));
    return benz_le64toh(x);
}

#undef BENZINA_PUBLIC_ENDIANOP
#undef BENZINA_PUBLIC_ENDIANOP_CONST
#undef BENZ_PICK_BE_LE

/* intops.h */

/**
 * @brief Bit rotation on integers.
 * 
 *   - rol: Bit Rotate Left
 *   - ror: Bit Rotate Right
 * 
 * @param [in] x  An integer.
 * @return The same integer, rotated by c bits over its bitwidth.
 */

BENZINA_ATTRIBUTE_CONST
BENZINA_ATTRIBUTE_ALWAYSINLINE
BENZINA_PUBLIC  BENZINA_INLINE
uint64_t benz_rol64(uint64_t x, unsigned c){
    const unsigned BW = 64;
    c &= BW-1;
    return !c ? x : x<<c | x>>(BW-c);
}

/* Data Structure Forward Declarations & Typedefs */
typedef struct BENZ_SIPHASH_STATE BENZ_SIPHASH_STATE;

/* ptrops.h */

/**
 * @brief Void pointer add/offset ((c)onst/(n)on-const)
 * 
 * @param [in]  p    (void) pointer to offset
 * @param [in]  off  Signed integer number of bytes.
 * @return p+off, as const/non-const void*
 */

BENZINA_ATTRIBUTE_CONST BENZINA_INLINE const void* benz_ptr_addcv(const void* p, int64_t  off){
    return (const void*)((const uint8_t*)p + off);
}
BENZINA_ATTRIBUTE_CONST BENZINA_INLINE void*       benz_ptr_addnv(void*       p, int64_t  off){
    return (      void*)((      uint8_t*)p + off);
}



/* Data Structure Definitions */

/**
 * @brief SipHash state
 * 
 * Size: 256 bits (4x 64-bit words)
 */

struct BENZ_SIPHASH_STATE{
    uint64_t v[4];
};



/* Static Function Definitions */
BENZINA_STATIC void benz_siphash_init(BENZ_SIPHASH_STATE* s, uint64_t k0, uint64_t k1){
    s->v[0] = k0 ^ 0x736f6d6570736575; /* "somepseu" */
    s->v[1] = k1 ^ 0x646f72616e646f6d; /* "dorandom" */
    s->v[2] = k0 ^ 0x6c7967656e657261; /* "lygenera" */
    s->v[3] = k1 ^ 0x7465646279746573; /* "tedbytes" */
}
BENZINA_STATIC void benz_siphash_rounds(BENZ_SIPHASH_STATE* s, uint64_t m, unsigned r){
    uint64_t v0 = s->v[0];
    uint64_t v1 = s->v[1];
    uint64_t v2 = s->v[2];
    uint64_t v3 = s->v[3];
    
    v3 ^= m;
    
    while(r--){
        #define rol(v, c) v = benz_rol64(v, c)
        /*   V0    ;     V1    ;     V2    ;     V3    ; */
        /* Half-round 1/2 start { v0&v1 | v2&v3 } */
        v0 += v1   ;           ; v2 += v3  ;           ;
                   ;rol(v1, 13);           ;rol(v3, 16);
                   ;v1 ^= v0   ;           ;v3 ^= v2   ;
        rol(v0, 32);           ;           ;           ;
        /* Half-round 1/2 end   { v0&v1 | v2&v3 } */
        /* Half-round 2/2 start { v0&v3 | v1&v2 } */
        v0 += v3   ;           ; v2 += v1  ;           ;
                   ;rol(v1, 17);           ;rol(v3, 21);
                   ;v1 ^= v2   ;           ;v3 ^= v0   ;
                   ;           ;rol(v2, 32);           ;
        /* Half-round 2/2 end   { v0&v3 | v1&v2 } */
        /*   V0    ;     V1    ;     V2    ;     V3    ; */
        #undef  rol
    }
    
    v0 ^= m;
    
    s->v[0] = v0;
    s->v[1] = v1;
    s->v[2] = v2;
    s->v[3] = v3;
}
BENZINA_STATIC uint64_t benz_siphash_padword(const void* buf, uint64_t len){
    uint64_t m=0;
    memcpy(&m, benz_ptr_addcv(buf, len&~7), len&7);
    return benz_getle64(&m, 0) | len<<56;
}
BENZINA_STATIC uint64_t benz_siphash_finalize(BENZ_SIPHASH_STATE* s, unsigned d){
    s->v[2] ^= 0xFFU;
    benz_siphash_rounds(s, 0, d);
    return s->v[0]^s->v[1]^s->v[2]^s->v[3];
}
BENZINA_STATIC uint64_t benz_siphash_digest_internal(const void* buf, uint64_t len,
                                                            uint64_t    k0,  uint64_t k1,
                                                            unsigned    c,   unsigned d){
    uint64_t l;
    BENZ_SIPHASH_STATE s;
    benz_siphash_init(&s, k0, k1);
    for(l=0;l+7<len;l+=8)
        benz_siphash_rounds(&s, benz_getle64(buf, l), c);
    benz_siphash_rounds(&s, benz_siphash_padword(buf, len), c);
    return benz_siphash_finalize(&s, d);
}



/* Public Function Definitions */
BENZINA_PUBLIC uint64_t benz_siphash_digestx(const void* buf, uint64_t len,
                                             uint64_t    k0,  uint64_t k1,
                                             unsigned    c,   unsigned d){
    return benz_siphash_digest_internal(buf, len, k0, k1, c, d);
}
BENZINA_PUBLIC uint64_t benz_siphash_digest (const void* buf, uint64_t len,
                                             uint64_t    k0,  uint64_t k1){
    return benz_siphash_digest_internal(buf, len, k0, k1, 2, 4);
}
