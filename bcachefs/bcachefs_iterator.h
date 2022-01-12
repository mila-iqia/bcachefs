/* Include Guard */
#ifndef INCLUDE_BCACHEFS_ITERATOR_H
#define INCLUDE_BCACHEFS_ITERATOR_H

/**
 * Includes
 */

#include "bcachefs.h"

/* Extern "C" Guard */
#ifdef __cplusplus
extern "C" {
#endif

//! Decoded value from the extend btree
typedef struct {
    uint64_t inode;
    uint64_t file_offset;
    uint64_t offset;
    uint64_t size;
} Bcachefs_extent;

//! Decoded value from the inode btree
typedef struct {
    uint64_t inode;
    uint64_t size;
    uint64_t hash_seed;
} Bcachefs_inode;

//! Decoded value from the dirent btree
typedef struct {
    uint64_t parent_inode;
    uint64_t inode;
    uint8_t type;
    const uint8_t *name;
    uint8_t name_len;
} Bcachefs_dirent;

typedef struct Bcachefs_iterator {
    enum btree_id type;                         //! which btree are we iterating over
    const struct jset_entry *jset_entry;        //! journal entry specifying the location of the btree root
    const struct bch_btree_ptr_v2 *btree_ptr;   //! current btree node location
    const struct bset *bset;                    //! current bset inside the btree
    const void *bkey;                           //! current bkey inside the bset
    const struct bch_val *bch_val;              //! current value stored inside along side the key
    struct btree_node *btree_node;              //! current btree node
    struct Bcachefs_iterator *next_it;          //! pointer to the children btree node if iterating over nested Btrees
    struct bset **_bsets;                       //! bsets pointers list inside the btree
    const struct bset **_bset;                  //! current bset in _bsets
    const struct bset **_bsets_end;             //! bsets list end pointer
} Bcachefs_iterator;
#define BCACHEFS_ITERATOR_CLEAN (Bcachefs_iterator){.type = BTREE_ID_NR}

typedef struct {
    FILE *fp;
    long size;
    struct bch_sb *sb;
    Bcachefs_iterator *_iter;
    Bcachefs_iterator _extents_iter_begin;
    Bcachefs_iterator _inodes_iter_begin;
    Bcachefs_iterator _dirents_iter_begin;
    Bcachefs_inode _root_stats;
    Bcachefs_dirent _root_dirent;
} Bcachefs;
#define BCACHEFS_CLEAN (Bcachefs){ \
    ._extents_iter_begin = BCACHEFS_ITERATOR_CLEAN, \
    ._inodes_iter_begin = BCACHEFS_ITERATOR_CLEAN, \
    ._dirents_iter_begin = BCACHEFS_ITERATOR_CLEAN, \
    ._root_stats = (Bcachefs_inode){0}, \
    ._root_dirent = (Bcachefs_dirent){0} \
}

/*! @brief Open a Bcachefs disk image for reading
 *
 *  @param [out] this Bcachefs struct to initialize
 *  @param [in] path path to the image
 *
 *  @return 1 on success, 0 on failure
 */
int Bcachefs_open(Bcachefs *this, const char *path);

/*! @brief Close a Bcachefs disk image
 *
 *  @param [in] this disk image to close
 *
 *  @return 1 on success, 0 on failure
 */
int Bcachefs_close(Bcachefs *this);

/*! @brief Create a Bcachefs iterator to go through a Bcachefs btree
 *
 *  @param [in] this disk image
 *  @param [in] type type of the btree to iterate over
 *
 *  @return initialized iterator struct or `NULL` on failure
 */
Bcachefs_iterator* Bcachefs_iter(const Bcachefs *this, enum btree_id type);

/*! @brief Find and parse an extent descriptor of a file at a particular offset
 *
 *         The file offset needs to exist in the extents list
 *
 *  @param [in] this disk image
 *  @param [in] inode inode of a file
 *  @param [in] file_offset offset of a file extent
 *
 *  @return parsed `Bcachefs_extent` or a zeroed struct on failure
 */
Bcachefs_extent Bcachefs_find_extent(Bcachefs *this, uint64_t inode, uint64_t file_offset);

/*! @brief Find and parse the inode informations of a file
 *
 *  @param [in] this disk image
 *  @param [in] inode inode of a file
 *
 *  @return parsed `Bcachefs_inode` or a zeroed struct on failure
 */
Bcachefs_inode Bcachefs_find_inode(Bcachefs *this, uint64_t inode);

/*! @brief Find and parse the dirent informations of a file
 *
 *  @param [in] this disk image
 *  @param [in] parent_inode inode of the parent directory
 *  @param [in] hash_seed hash seed of the parent directory or 0
 *  @param [in] name name of the dirent to find
 *  @param [in] len length of the name string
 *
 *  @return parsed `Bcachefs_inode` or a zeroed struct on failure
 */
Bcachefs_dirent Bcachefs_find_dirent(Bcachefs *this, uint64_t parent_inode, uint64_t hash_seed, const uint8_t *name, const uint8_t len);

/*! @brief Fetch next value from an iterator
 *
 *  @param [in] this disk image
 *  @param [in] iter a disk image's iterator struct
 *
 *  @return next `bch_val` or `NULL` if the end is reached 
 *
 *  @todo return a `bkey` instead of a `bch_val` to make this return value more useful
 */
const struct bch_val *Bcachefs_iter_next(const Bcachefs *this, Bcachefs_iterator *iter);

/*! @brief Free the resources allocated by the iterator
 *
 *  @param [in] this disk image
 *  @param [in] iter a disk image's iterator struct
 *
 *  @return 1 on success, 0 on failure
 */
int Bcachefs_iter_fini(const Bcachefs *this, Bcachefs_iterator *iter);

/*! @brief Extract extent descriptor from the current `bch_val` of an iterator
 *
 *  @param [in] this disk image
 *  @param [in] iter a disk image's iterator struct
 *
 *  @return parsed `Bcachefs_extent` or a zeroed struct on failure
 */
Bcachefs_extent Bcachefs_iter_make_extent(const Bcachefs *this, Bcachefs_iterator *iter);

/*! @brief Extract inode information from the current `bch_val` of an iterator
 *
 *  @param [in] this disk image
 *  @param [in] iter a disk image's iterator struct
 *
 *  @return parsed `Bcachefs_inode` or a zeroed struct on failure
 */
Bcachefs_inode Bcachefs_iter_make_inode(const Bcachefs *this, Bcachefs_iterator *iter);

/*! @brief Extract dirent information from the current `bch_val` of an iterator
 *
 *  @param [in] this the disk image we are reading from
 *  @param [in] iter the iterator pointing to the value we want to extract
 *
 *  @return parsed `Bcachefs_dirent` or a zeroed struct on failure
 */
Bcachefs_dirent Bcachefs_iter_make_dirent(const Bcachefs *this, Bcachefs_iterator *iter);

int Bcachefs_next_iter(const Bcachefs *this, Bcachefs_iterator *iter, const struct bch_btree_ptr_v2 *btree_ptr);
int Bcachefs_iter_reinit(const Bcachefs *this, Bcachefs_iterator *iter, enum btree_id type);
int Bcachefs_iter_minimal_copy(const Bcachefs *this, Bcachefs_iterator *iter, const Bcachefs_iterator *other);
const struct jset_entry *Bcachefs_iter_next_jset_entry(const Bcachefs *this, Bcachefs_iterator *iter);
const struct bch_btree_ptr_v2 *Bcachefs_iter_next_btree_ptr(const Bcachefs *this, Bcachefs_iterator *iter);
const struct bset *Bcachefs_iter_next_bset(const Bcachefs *this, Bcachefs_iterator *iter);

/* End Extern "C" and Include Guard */
#ifdef __cplusplus
}
#endif
#endif
