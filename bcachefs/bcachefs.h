/* Include Guard */
#ifndef INCLUDE_BCACHEFS_H
#define INCLUDE_BCACHEFS_H


/**
 * Includes
 */

#include <stdio.h>

#include "utils.h"

/* Extern "C" Guard */
#ifdef __cplusplus
extern "C" {
#endif

/* Defines */

#define BCH_SB_SECTOR           8
#define BCH_SB_LABEL_SIZE       32
#define BCH_SECTOR_SIZE         512
#define BCH_U64S_SIZE           8

#define BCH_SB_FIELDS()             \
    x(journal,      0)              \
    x(members,      1)              \
    x(crypt,        2)              \
    x(replicas_v0,  3)              \
    x(quota,        4)              \
    x(disk_groups,  5)              \
    x(clean,        6)              \
    x(replicas,     7)              \
    x(journal_seq_blacklist, 8)

#define BCH_JSET_ENTRY_TYPES()      \
    x(btree_keys,       0)          \
    x(btree_root,       1)          \
    x(prio_ptrs,        2)          \
    x(blacklist,        3)          \
    x(blacklist_v2,     4)          \
    x(usage,            5)          \
    x(data_usage,       6)          \
    x(clock,            7)          \
    x(dev_usage,        8)

/* Btree: */

#define BCH_BTREE_IDS()             \
    x(extents,          0)          \
    x(inodes,           1)          \
    x(dirents,          2)          \
    x(xattrs,           3)          \
    x(alloc,            4)          \
    x(quotas,           5)          \
    x(stripes,          6)          \
    x(reflink,          7)

/*
 * - DELETED keys are used internally to mark keys that should be ignored but
 *   override keys in composition order.  Their version number is ignored.
 *
 * - DISCARDED keys indicate that the data is all 0s because it has been
 *   discarded. DISCARDs may have a version; if the version is nonzero the key
 *   will be persistent, otherwise the key will be dropped whenever the btree
 *   node is rewritten (like DELETED keys).
 *
 * - ERROR: any read of the data returns a read error, as the data was lost due
 *   to a failing device. Like DISCARDED keys, they can be removed (overridden)
 *   by new writes or cluster-wide GC. Node repair can also overwrite them with
 *   the same or a more recent version number, but not with an older version
 *   number.
 *
 * - WHITEOUT: for hash table btrees
*/
#define BCH_BKEY_TYPES()            \
    x(deleted,          0)          \
    x(discard,          1)          \
    x(error,            2)          \
    x(cookie,           3)          \
    x(hash_whiteout,    4)          \
    x(btree_ptr,        5)          \
    x(extent,           6)          \
    x(reservation,      7)          \
    x(inode,            8)          \
    x(inode_generation, 9)          \
    x(dirent,           10)         \
    x(xattr,            11)         \
    x(alloc,            12)         \
    x(quota,            13)         \
    x(stripe,           14)         \
    x(reflink_p,        15)         \
    x(reflink_v,        16)         \
    x(inline_data,      17)         \
    x(btree_ptr_v2,     18)         \
    x(indirect_inline_data, 19)     \
    x(alloc_v2,         20)

#define BCH_EXTENT_ENTRY_TYPES()    \
    x(ptr,              0)          \
    x(crc32,            1)          \
    x(crc64,            2)          \
    x(crc128    ,       3)          \
    x(stripe_ptr,       4)
#define BCH_EXTENT_ENTRY_MAX        5

#define BKEY_U64s                   (sizeof(struct bkey) / sizeof(uint64_t))
#define KEY_FORMAT_LOCAL_BTREE      0
#define KEY_FORMAT_CURRENT          1

/*
 * Magic numbers
 *
 * The various other data structures have their own magic numbers, which are
 * xored with the first part of the cache set's UUID
 */

#define BCH_KEY_MAGIC                                   \
    (((uint64_t) 'b' <<  0)|((uint64_t) 'c' <<  8)|     \
     ((uint64_t) 'h' << 16)|((uint64_t) '*' << 24)|     \
     ((uint64_t) '*' << 32)|((uint64_t) 'k' << 40)|     \
     ((uint64_t) 'e' << 48)|((uint64_t) 'y' << 56))

#define JSET_MAGIC      0x245235c1a3625032ULL
#define BSET_MAGIC      0x90135c78b99e07f5ULL

enum bch_sb_field_type {
#define x(f, nr)    BCH_SB_FIELD_##f = nr,
    BCH_SB_FIELDS()
#undef x
    BCH_SB_FIELD_NR
};

enum bch_jset_entry_type {
#define x(f, nr)    BCH_JSET_ENTRY_##f  = nr,
    BCH_JSET_ENTRY_TYPES()
#undef x
    BCH_JSET_ENTRY_NR
};

enum btree_id {
#define x(kwd, val) BTREE_ID_##kwd = val,
    BCH_BTREE_IDS()
#undef x
    BTREE_ID_NR
};

enum bch_bkey_fields {
    BKEY_FIELD_INODE,
    BKEY_FIELD_OFFSET,
    BKEY_FIELD_SNAPSHOT,
    BKEY_FIELD_SIZE,
    BKEY_FIELD_VERSION_HI,
    BKEY_FIELD_VERSION_LO,
    BKEY_NR_FIELDS,
};

enum bch_bkey_type {
#define x(name, nr) KEY_TYPE_##name = nr,
    BCH_BKEY_TYPES()
#undef x
    KEY_TYPE_MAX,
};

enum bch_extent_entry_type {
#define x(f, n) BCH_EXTENT_ENTRY_##f = n,
    BCH_EXTENT_ENTRY_TYPES()
#undef x
};

enum bch_inode_flags{
    BCH_INODE_FLAG_sync              = (1UL <<  0),
    BCH_INODE_FLAG_immutable         = (1UL <<  1),
    BCH_INODE_FLAG_append            = (1UL <<  2),
    BCH_INODE_FLAG_nodump            = (1UL <<  3),
    BCH_INODE_FLAG_noatime           = (1UL <<  4),
    BCH_INODE_FLAG_i_size_dirty      = (1UL <<  5),
    BCH_INODE_FLAG_i_sectors_dirty   = (1UL <<  6),
    BCH_INODE_FLAG_unlinked          = (1UL <<  7),
    BCH_INODE_FLAG_backptr_untrusted = (1UL <<  8),
    BCH_INODE_FLAG_new_varint        = (1UL << 31),
};
struct u64s_spec {
    uint32_t size;  /* size in bytes of the u64s field */
    uint32_t start; /* should be added to the u64s field */
};

static const struct u64s_spec U64S_BCH_SB_FIELD = {
    .size = sizeof(uint32_t),
    .start = 0};
static const struct u64s_spec U64S_JSET_ENTRY = {
    .size = sizeof(uint16_t),
    .start = 1};
static const struct u64s_spec U64S_BKEY_I = {
    .size = sizeof(uint8_t),
    .start = 0};
static const struct u64s_spec U64S_BKEY = {
    .size = sizeof(uint8_t),
    .start = 0};

struct uuid {
    uint8_t         bytes[16];
} __attribute__((packed, aligned(8)));

/* 128 bits, sufficient for cryptographic MACs: */
struct bch_csum {
    uint64_t        lo;
    uint64_t        hi;
} __attribute__((packed, aligned(8)));

struct bch_sb_layout {
    struct uuid     magic;  /* bcachefs superblock UUID */
    uint8_t         layout_type;
    uint8_t         sb_max_size_bits; /* base 2 of 512 byte sectors */
    uint8_t         nr_superblocks;
    uint8_t         pad[5];
    uint64_t        sb_offset[61];
} __attribute__((packed, aligned(8)));

/* Optional/variable size superblock sections: */

struct bch_sb_field {
    uint64_t        _data[0];
    uint32_t        u64s;
    uint32_t        type;
};

/*
 * @offset  - sector where this sb was written
 * @version - on disk format version
 * @version_min - Oldest metadata version this filesystem contains; so we can
 *        safely drop compatibility code and refuse to mount filesystems
 *        we'd need it for
 * @magic   - identifies as a bcachefs superblock (BCACHE_MAGIC)
 * @seq     - incremented each time superblock is written
 * @uuid    - used for generating various magic numbers and identifying
 *                member devices, never changes
 * @user_uuid   - user visible UUID, may be changed
 * @label   - filesystem label
 * @seq     - identifies most recent superblock, incremented each time
 *        superblock is written
 * @features    - enabled incompatible features
 */
struct bch_sb {
    struct bch_csum     csum;
    uint16_t        version;
    uint16_t        version_min;
    uint16_t        pad[2];
    struct uuid     magic;
    struct uuid     uuid;
    struct uuid     user_uuid;
    uint8_t         label[BCH_SB_LABEL_SIZE];
    uint64_t        offset;
    uint64_t        seq;

    uint16_t        block_size;
    uint8_t         dev_idx;
    uint8_t         nr_devices;
    uint32_t        u64s;

    uint64_t        time_base_lo;
    uint32_t        time_base_hi;
    uint32_t        time_precision;

    uint64_t        flags[8];
    uint64_t        features[2];
    uint64_t        compat[2];

    struct bch_sb_layout    layout;

    union {
        struct bch_sb_field start[0];
        uint64_t    _data[0];
    };
} __attribute__((packed, aligned(8)));

/* Btree keys - all units are in sectors */

struct bversion {
    uint32_t        hi;
    uint64_t        lo;
} __attribute__((packed, aligned(4)));

struct bpos {
    /*
     * Word order matches machine byte order - btree code treats a bpos as a
     * single large integer, for search/comparison purposes
     *
     * Note that wherever a bpos is embedded in another on disk data
     * structure, it has to be byte swabbed when reading in metadata that
     * wasn't written in native endian order:
     */
    uint32_t        snapshot;
    uint64_t        offset;     /* Points to end of extent - sectors */
    uint64_t        inode;
} __attribute__((packed, aligned(4)));

static inline struct bpos SPOS(uint64_t inode, uint64_t offset, uint32_t snapshot)
{
    return (struct bpos) {
        .inode      = inode,
        .offset     = offset,
        .snapshot   = snapshot,
    };
}

/* Empty placeholder struct, for container_of() */
struct bch_val {
    uint64_t        __nothing[0];
};

struct bkey_format {
    uint8_t     key_u64s;
    uint8_t     nr_fields;
    /* One unused slot for now: */
    uint8_t     bits_per_field[6];
    uint64_t    field_offset[6];
};

static const struct bkey_format BKEY_FORMAT_SHORT = {
    .key_u64s = 3,
    .nr_fields = 6,
    .bits_per_field = {64,  // BKEY_FIELD_INODE
                       64,  // BKEY_FIELD_OFFSET
                       32,  // BKEY_FIELD_SNAPSHOT
                       0, 0, 0},
    .field_offset = {0}
};

struct bkey_short {
    /* Size of combined key and value, in u64s */
    uint8_t     u64s;

    /* Format of key (0 for format local to btree node) */
    uint8_t     format:7,
                needs_whiteout:1;

    /* Type of the value */
    uint8_t     type;

    uint8_t     pad[1];

//    struct bversion version;
//    uint32_t    size;       /* extent size, in sectors */
    struct bpos p;
} __attribute__((packed, aligned(8)));

struct bkey_local {
    /* Size of combined key and value, in u64s */
    uint8_t     u64s;

    /* Format of key (0 for format local to btree node) */
    uint8_t     format:7,
                needs_whiteout:1;

    /* Type of the value */
    uint8_t     type;

    uint8_t     pad[1];

    struct bversion version;
    uint32_t    size;       /* extent size, in sectors */
    struct bpos p;

    uint8_t     key_u64s;
} __attribute__((packed, aligned(8)));

struct bkey_local_buffer {
    uint64_t    buffer[BKEY_NR_FIELDS];

    uint8_t     key_u64s;
};

struct bkey {
    /* Size of combined key and value, in u64s */
    uint8_t     u64s;

    /* Format of key (0 for format local to btree node) */
    uint8_t     format:7,
                needs_whiteout:1;

    /* Type of the value */
    uint8_t     type;

    uint8_t     pad[1];

    struct bversion version;
    uint32_t    size;       /* extent size, in sectors */
    struct bpos p;
} __attribute__((packed, aligned(8)));

struct bkey_packed {
    uint64_t    _data[0];

    /* Size of combined key and value, in u64s */
    uint8_t     u64s;

    /* Format of key (0 for format local to btree node) */

    /*
     * XXX: next incompat on disk format change, switch format and
     * needs_whiteout - bkey_packed() will be cheaper if format is the high
     * bits of the bitfield
     */
    uint8_t     format:7,
                needs_whiteout:1;

    /* Type of the value */
    uint8_t     type;
    uint8_t     key_start[0];

    /*
     * We copy bkeys with struct assignment in various places, and while
     * that shouldn't be done with packed bkeys we can't disallow it in C,
     * and it's legal to cast a bkey to a bkey_packed  - so padding it out
     * to the same size as struct bkey should hopefully be safest.
     */
    uint8_t     pad[sizeof(struct bkey) - 3];
} __attribute__((packed, aligned(8)));

/* bkey with inline value */
struct bkey_i {
    uint64_t    _data[0];

    union {
    struct {
        /* Size of combined key and value, in u64s */
        uint8_t u64s;
    };
    struct {
        struct bkey k;
        struct bch_val  v;
    };
    };
};

/*
 * On clean shutdown, store btree roots and current journal sequence number in
 * the superblock:
 */
struct jset_entry {
    uint16_t    u64s;
    uint8_t     btree_id;
    uint8_t     level;
    uint8_t     type; /* designates what this jset holds */
    uint8_t     pad[3];

    union {
        struct bkey_i   start[0];
        uint64_t    _data[0];
    };
};

struct bch_sb_field_clean {
    struct bch_sb_field field;

    uint32_t    flags;
    uint16_t    _read_clock; /* no longer used */
    uint16_t    _write_clock;
    uint64_t    journal_seq;

    union {
        struct jset_entry start[0];
        uint64_t    _data[0];
    };
};

/* Extents */

/* Compressed/uncompressed size are stored biased by 1: */
struct bch_extent_crc32 {
    uint32_t    type:2,
                _compressed_size:7,
                _uncompressed_size:7,
                offset:7,
                _unused:1,
                csum_type:4,
                compression_type:4;
    uint32_t    csum;
} __attribute__((packed, aligned(8)));

#define CRC32_SIZE_MAX      (1U << 7)
#define CRC32_NONCE_MAX     0

struct bch_extent_crc64 {
    uint64_t    type:3,
                _compressed_size:9,
                _uncompressed_size:9,
                offset:9,
                nonce:10,
                csum_type:4,
                compression_type:4,
                csum_hi:16;
    uint64_t    csum_lo;
} __attribute__((packed, aligned(8)));

#define CRC64_SIZE_MAX      (1U << 9)
#define CRC64_NONCE_MAX     ((1U << 10) - 1)

struct bch_extent_crc128 {
    uint64_t    type:4,
                _compressed_size:13,
                _uncompressed_size:13,
                offset:13,
                nonce:13,
                csum_type:4,
                compression_type:4;
    struct bch_csum     csum;
} __attribute__((packed, aligned(8)));

#define CRC128_SIZE_MAX     (1U << 13)
#define CRC128_NONCE_MAX    ((1U << 13) - 1)

/*
 * @reservation - pointer hasn't been written to, just reserved
 */
struct bch_extent_ptr {
    uint64_t    type:1,
                cached:1,
                unused:1,
                reservation:1,
                offset:44, /* 8 petabytes */
                dev:8,
                gen:8;
} __attribute__((packed, aligned(8)));

struct bch_extent_stripe_ptr {
    uint64_t    type:5,
                block:8,
                redundancy:4,
                idx:47;
};

struct bch_extent_reservation {
    uint64_t    type:6,
                unused:22,
                replicas:4,
                generation:32;
};

union bch_extent_entry {
    unsigned long           type;

#define x(f, n) struct bch_extent_##f   f;
    BCH_EXTENT_ENTRY_TYPES()
#undef x
};

struct bch_btree_ptr_v2 {
    struct bch_val      v;

    uint64_t    mem_ptr;
    uint64_t    seq;
    uint16_t    sectors_written;
    uint16_t    flags;
    struct bpos min_key;
    struct bch_extent_ptr   start[0];
    uint64_t    _data[0];
} __attribute__((packed, aligned(8)));

struct bch_extent {
    struct bch_val      v;

    union bch_extent_entry  start[0];
    uint64_t    _data[0];
} __attribute__((packed, aligned(8)));

/* Inodes */

#define BCACHEFS_ROOT_INO   4096

struct bch_inode {
    struct bch_val      v;

    uint64_t    bi_hash_seed;
    uint32_t    bi_flags;
    uint16_t    bi_mode;
    uint8_t     fields[0];
} __attribute__((packed, aligned(8)));

/* Dirents */

/*
 * Dirents (and xattrs) have to implement string lookups; since our b-tree
 * doesn't support arbitrary length strings for the key, we instead index by a
 * 64 bit hash (currently truncated sha1) of the string, stored in the offset
 * field of the key - using linear probing to resolve hash collisions. This also
 * provides us with the readdir cookie posix requires.
 *
 * Linear probing requires us to use whiteouts for deletions, in the event of a
 * collision:
 */

struct bch_dirent {
    struct bch_val      v;

    /* Target inode number: */
    uint64_t    d_inum;

    /*
     * Copy of mode bits 12-15 from the target inode - so userspace can get
     * the filetype without having to do a stat()
     */
    uint8_t     d_type;

    uint8_t     d_name[];
} __attribute__((packed, aligned(8)));

/* Inline data */

struct bch_inline_data {
    struct bch_val      v;
    uint8_t     data[0];
};

/* Btree nodes */

/*
 * Btree nodes
 *
 * On disk a btree node is a list/log of these; within each set the keys are
 * sorted
 */
struct bset {
    uint64_t    seq;

    /*
     * Highest journal entry this bset contains keys for.
     * If on recovery we don't see that journal entry, this bset is ignored:
     * this allows us to preserve the order of all index updates after a
     * crash, since the journal records a total order of all index updates
     * and anything that didn't make it to the journal doesn't get used.
     */
    uint64_t    journal_seq;

    uint32_t    flags;
    uint16_t    version;
    uint16_t    u64s; /* count of d[] in u64s */

    union {
        struct bkey_packed start[0];
        uint64_t        _data[0];
    };
} __attribute__((packed, aligned(8)));

struct btree_node {
    struct bch_csum     csum;
    uint64_t    magic;

    /* this flags field is encrypted, unlike bset->flags: */
    uint64_t    flags;

    /* Closed interval: */
    struct bpos     min_key;
    struct bpos     max_key;
    struct bch_extent_ptr   _ptr; /* not used anymore */
    struct bkey_format  format;

    union {
    struct bset     keys;
    struct {
        uint8_t     pad[22];
        uint16_t    u64s;
        uint64_t    _data[0];

    };
    };
} __attribute__((packed, aligned(8)));

struct btree_node_entry {
    struct bch_csum     csum;

    union {
    struct bset     keys;
    struct {
        uint8_t     pad[22];
        uint16_t    u64s;
        uint64_t    _data[0];

    };
    };
} __attribute__((packed, aligned(8)));

static inline uint64_t __bch2_sb_magic(struct bch_sb *sb)
{
    uint64_t ret;
    memcpy(&ret, &sb->uuid, sizeof(ret));
    return ret;
}

static inline uint64_t __jset_magic(struct bch_sb *sb)
{
    return __bch2_sb_magic(sb) ^ JSET_MAGIC;
}

static inline uint64_t __bset_magic(struct bch_sb *sb)
{
    return __bch2_sb_magic(sb) ^ BSET_MAGIC;
}

static const struct uuid BCACHE_MAGIC = {{
    0xc6, 0x85, 0x73, 0xf6,
    0x4e, 0x1a,
    0x45, 0xca,
    0x82, 0x65,
    0xf5, 0x7f, 0x48, 0xba, 0x6d, 0x81}};

const void *benz_bch_next_sibling(const void *p, uint32_t sizeof_p, const void *p_end, const void *c, struct u64s_spec u64s_spec);

const struct bch_sb_field *benz_bch_next_sb_field(const struct bch_sb *p, const struct bch_sb_field *c, enum bch_sb_field_type type);
const struct jset_entry *benz_bch_next_jset_entry(const struct bch_sb_field *p,
                                                  uint32_t sizeof_p,
                                                  const struct jset_entry *c,
                                                  enum bch_jset_entry_type type);
const struct bch_val *benz_bch_first_bch_val(const struct bkey *p, uint8_t key_u64s);
const struct bch_val *benz_bch_next_bch_val(const struct bkey *p, const struct bch_val *c, uint32_t sizeof_c);
const struct bset *benz_bch_next_bset(const struct btree_node *p, const void *p_end, const struct bset *c, const struct bch_sb *sb);
const struct bkey *benz_bch_next_bkey(const struct bset *p, const struct bkey *c, enum bch_bkey_type type);

struct bkey_local benz_bch_parse_bkey(const struct bkey *bkey, const struct bkey_local_buffer *buffer);
struct bkey_local_buffer benz_bch_parse_bkey_buffer(const struct bkey *bkey, const struct bkey_format *format, enum bch_bkey_fields fields_cnt);
uint64_t benz_bch_parse_bkey_field(const struct bkey *bkey, const struct bkey_format *format, enum bch_bkey_fields field);

uint64_t benz_bch_get_sb_size(const struct bch_sb *sb);
uint64_t benz_bch_get_block_size(const struct bch_sb *sb);
uint64_t benz_bch_get_btree_node_size(const struct bch_sb *sb);
uint64_t benz_bch_get_extent_offset(const struct bch_extent_ptr *bch_extent_ptr);

const struct bkey *benz_bch_file_offset_size(const struct bkey *bkey,
                                             const struct bch_val *bch_val,
                                             uint64_t *file_offset,
                                             uint64_t *offset,
                                             uint64_t *size);
uint64_t benz_bch_inline_data_offset(const struct btree_node* start, const struct bch_val *bch_val, uint64_t start_offset);

struct bch_sb *benz_bch_realloc_sb(struct bch_sb *sb, uint64_t size);
struct btree_node *benz_bch_malloc_btree_node(const struct bch_sb *sb);

uint64_t benz_bch_fread_sb(struct bch_sb *sb, uint64_t size, FILE *fp);
uint64_t benz_bch_fread_btree_node(struct btree_node *btree_node, const struct bch_sb *sb, const struct bch_btree_ptr_v2 *btree_ptr, FILE *fp);

void benz_print_uuid(const struct uuid *uuid);

/* End Extern "C" and Include Guard */
#ifdef __cplusplus
}
#endif
#endif
