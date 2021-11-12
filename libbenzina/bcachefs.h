/* Include Guard */
#ifndef INCLUDE_BENZINA_BCACHEFS_H
#define INCLUDE_BENZINA_BCACHEFS_H

/**
 * Includes
 */

#include "bcachefs/bcachefs.h"


/* Extern "C" Guard */
#ifdef __cplusplus
extern "C" {
#endif

int   benz_bch_inode_unpack_size(uint64_t*               bi_size,
                                 const struct bch_inode* p,
                                 const void*             end);

/* End Extern "C" and Include Guard */
#ifdef __cplusplus
}
#endif
#endif
