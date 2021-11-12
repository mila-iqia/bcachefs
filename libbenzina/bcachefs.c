#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "libbenzina/bcachefs.h"


#pragma GCC push_options
#pragma GCC optimize ("O2")

static uint32_t benz_getle32(const void* p, int64_t off){
    uint32_t x;
    memcpy(&x, (char*)p+off, sizeof(x));
    return x;
}

static uint64_t benz_getle64(const void* p, int64_t off){
    uint64_t x;
    memcpy(&x, (char*)p+off, sizeof(x));
    return x;
}

#define luai_unlikely(x) x

unsigned benz_ctz64(uint64_t x){
    return x ? __builtin_ctzll(x) : 64;
}

int   benz_bch_inode_unpack_size(uint64_t*               bi_size,
                                 const struct bch_inode* p,
                                 const void*             end){
    register int      new_varint, nr_fields;
    register int      varintc;
    register uint32_t bi_flags;
    register uint64_t f;
    const uint8_t* e = (const uint8_t*)end;
    const uint8_t* r = (const uint8_t*)&p->fields;

    *bi_size = 0;/* Default is 0. */
    if(e<r)
        return -1;/* Parse error, end pointer behind field pointer! */

    bi_flags   = benz_getle32(&p->bi_flags, 0);
    nr_fields  = (int)(bi_flags >> 24) & 127;
    new_varint = !!(bi_flags & BCH_INODE_FLAG_new_varint);

    if(!new_varint)
        return -2;/* Parse error, old-style varint! */

    if(e-r < (ptrdiff_t)nr_fields)
        return -3;/* Parse error, end pointer far too short! At least 1 byte/field. */

    /**
     * The field bi_size is the 5th field and 9th varint in a v2-packed inode,
     * being preceded by four wide (double-varint) fields (the 96-bit timestamps).
     *
     * Accordingly, check that the number of fields is at least 5, and if so
     * then scan up to the 9th varint.
     */

    if(nr_fields < 5)
        return  -4;/* No size field encoded, default is 0. */

    for(varintc=0; varintc<9; varintc++){
        f  = benz_ctz64(*r+1)+1;
        r += f;
        if(luai_unlikely(r>e))
            return -5;
    }

    /**
     * Pointer r now points one byte past the end of the target varint.
     * Decode varint at current location.
     */

    f *= 6;
    f &= 0x3F;/* Can be elided on x86_64 */
    /* For field length:   9  8  7  6  5  4  3  2  1    */
    /* Shift right by: --  0  8 15 22 29 36 43 50 57 -- */
    f  = 000101726354453627100 >> f;
    f &= 0x3F;/* Can be elided on x86_64 */
    f  = benz_getle64(r,-8) >> f;
    *bi_size = f;

    return 0;
}
#pragma GCC pop_options
