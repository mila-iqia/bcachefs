#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "bcachefs.h"

#include "siphash.h"

// Our data structure structs are really just header of contiguous lists.  Most
// of the time, the header always start with the size of full list in bytes
//
// The elements are starting after the header struct, they can have different
// size, their size is also stored in the first few bytes of the element u64s
// can have different sizes (uint8_t to uint64_t), to know the number of bytes
// used to store the size we need to use the `struct u64s_spec`
//
//  * struct parent_header {
//  |  u64s		// Size of the entire data structure in `BCH_U64S_SIZE`
//  |  metadata
//  |     .
//  |     .
//  |     .
//  |  metadata
//  | };
//  | *  struct child {
//  | |   u64s          // Size of this value in `BCH_U64S_SIZE`
//  | +> };
//  | *  struct child {
//  | |   u64s          // Size of this value in `BCH_U64S_SIZE`
//  | +> };
//  +>                  // End of data structure
//
//  end         = &parent_header + header->u64s;
//  first_value = &parent_header + sizeof(header);


// Reads the u64s field contained inside a struct (assume it is the first
// field) note that fields like `uint64_t _data[0];` do not contribute to the
// struct size and create a pointer to the begining of the struct.
//
// TODO: duplicate with benz_uintXX_as_uint64
uint64_t read_u64s(const void *c, struct u64s_spec u64s_spec) {
    uint64_t u64s = 0;
    switch (u64s_spec.size)
    {
    case sizeof(uint8_t):
        u64s = *((const uint8_t*)c);
        break;
    case sizeof(uint16_t):
        u64s = *((const uint16_t*)c);
        break;
    case sizeof(uint32_t):
        u64s = *((const uint32_t*)c);
        break;
    case sizeof(uint64_t):
        u64s = *((const uint64_t*)c);
        break;
    default:
        u64s = 0;
    }
    return u64s;
}

// Gets next element, reads the size of the current element and jump to the
// next one
//
// if current element is null, set it to the first element
// if current element is higher than the end returns null
//
// p            : start of our parent data structure
// size_of_p    : size of the header of our parent data structure
// p_end        : end of the elements
// c            : current child element
// u64s_spec    : number of bytes used to store the element size
const void *benz_bch_next_sibling(const void *p, uint32_t sizeof_p, const void *p_end, const void *c, struct u64s_spec u64s_spec)
{
    if (c == NULL)
    {
        // if null fetch first element which is right after the header
        c = (const uint8_t*)p + sizeof_p;
    }
    else
    {
        // fetch next element by reading the size of current element and jumping to the
        // next one
        uint64_t u64s = read_u64s(c, u64s_spec) + u64s_spec.start;
        c = (const uint8_t*)c + u64s * BCH_U64S_SIZE;
    }

    // if we reached the end simply return null
    if (c >= p_end)
    {
        c = NULL;
    }
    return c;
}

// Iterate through superblock field looking for a specific field type. If `type
// == BCH_SB_FIELD_NR` then next field is returned
const struct bch_sb_field *benz_bch_next_sb_field(const struct bch_sb *p, const struct bch_sb_field *c, enum bch_sb_field_type type)
{
    const uint8_t *p_end = (const uint8_t*)p + p->u64s * BCH_U64S_SIZE;
    do
    {
        c = (const struct bch_sb_field*)benz_bch_next_sibling(p, sizeof(*p), p_end, c, U64S_BCH_SB_FIELD);
    } while (c && type != BCH_SB_FIELD_NR && c->type != type);
    return c;
}

// Iterate through journal set entries looking for a specific field type. If
// `type == BCH_JSET_ENTRY_NR` then next entry is returned
const struct jset_entry *benz_bch_next_jset_entry(const struct bch_sb_field *p,
                                                  uint32_t sizeof_p,
                                                  const struct jset_entry *c,
                                                  enum bch_jset_entry_type type)
{
    const uint8_t *p_end = (const uint8_t*)p + p->u64s * BCH_U64S_SIZE;
    do
    {
        c = (const struct jset_entry*)benz_bch_next_sibling(p, sizeof_p, p_end, c, U64S_JSET_ENTRY);
    } while (c && type != BCH_JSET_ENTRY_NR && c->type != type);
    return c;
}

// Returns the first value held by a bkey
const struct bch_val *benz_bch_first_bch_val(const struct bkey *p, uint8_t key_u64s)
{
    const struct bch_val *p_end = (const void*)((const uint8_t*)p + p->u64s * BCH_U64S_SIZE);
    const struct bch_val *c = (const void*)((const uint8_t*)p + key_u64s * BCH_U64S_SIZE);
    if (c >= p_end)
    {
        c = NULL;
    }
    return c;
}

// This is actually returning next btree pointer when we already have one
//
// p       : is our initial parent entry
// c       : is our child
// sizeof_c: is the size of child
const struct bch_val *benz_bch_next_bch_val(const struct bkey *p, const struct bch_val *c, uint32_t sizeof_c)
{
    const struct bch_val *p_end = (const void*)((const uint8_t*)p + p->u64s * BCH_U64S_SIZE);
    c = (const void*)((const uint8_t*)c + sizeof_c);
    if (c >= p_end)
    {
        c = NULL;
    }
    return c;
}

// Fetch next valid bset
const struct bset *benz_bch_next_bset(const struct btree_node *p, const void *p_end, const struct bset *c, const struct bch_sb *sb)
{
    uint64_t block_size = benz_bch_get_block_size(sb);
    do
    {
        if (c == NULL)
        {
            c = &p->keys;
        }
        else
        {
            // We want to find the next bset which is located at the next
            // block_size from the beginning of parent. It is possible for
            // `(uint64_t)p % block_size == 0` to always be true but in case it
            // could not be, reposition _cb to be relative to the beginning of
            // p when looking for the next block_size location, then move back
            // to the correct location in RAM
            const uint8_t *_cb = (const uint8_t*)c;
            _cb -= (uint64_t)p;

            // next bset
            _cb += sizeof(*c) + c->u64s * BCH_U64S_SIZE;

            // bset starts at a blocksize
            _cb += block_size - (uint64_t)_cb % block_size +
                   // skip btree_node_entry csum
                   sizeof(struct bch_csum);

            _cb += (uint64_t)p;
            c = (const void*)_cb;
        }
        if ((const void*)c >= p_end)
        {
            c = NULL;
        }
    } while (c && !c->u64s);
    return c;
}

// Iterate through bkeys inside a bset looking for a specific key type if `type
// == KEY_TYPE_MAX` then next key is returned
const struct bkey *benz_bch_next_bkey(const struct bset *p, const struct bkey *c, enum bch_bkey_type type)
{
    const uint8_t *p_end = (const uint8_t*)p->start + p->u64s * BCH_U64S_SIZE;
    do
    {
        c = (const struct bkey*)benz_bch_next_sibling(p, sizeof(*p), p_end, c, U64S_BKEY);
    } while (c && type != KEY_TYPE_MAX && c->type != type);
    return c;
}

struct bkey_local benz_bch_parse_bkey(const struct bkey *bkey, const struct bkey_local_buffer *buffer)
{
    struct bkey_local local = {.u64s = bkey->u64s,
                               .format = bkey->format,
                               .needs_whiteout = bkey->needs_whiteout,
                               .type = bkey->type};
    if (bkey->format == KEY_FORMAT_LOCAL_BTREE)
    {
        const uint64_t *value = buffer->buffer;
        for (enum bch_bkey_fields i = 0; i < BKEY_NR_FIELDS; ++i, ++value)
        {
            switch ((int)i)
            {
            case BKEY_FIELD_INODE:
                local.p.inode = *value;
                break;
            case BKEY_FIELD_OFFSET:
                local.p.offset = *value;
                break;
            case BKEY_FIELD_SNAPSHOT:
                local.p.snapshot = (uint32_t)*value;
                break;
            case BKEY_FIELD_SIZE:
                local.size = (uint32_t)*value;
                break;
            case BKEY_FIELD_VERSION_HI:
                local.version.hi = (uint32_t)*value;
                break;
            case BKEY_FIELD_VERSION_LO:
                local.version.lo = *value;
                break;
            }
        }
        local.key_u64s = buffer->key_u64s;
    }
    else if (bkey->format == KEY_FORMAT_CURRENT)
    {
        memcpy(&local, bkey, sizeof(*bkey));
        local.key_u64s = BKEY_U64s;
    }
    return local;
}

struct bkey_local_buffer benz_bch_parse_bkey_buffer(const struct bkey *bkey, const struct bkey_format *format, enum bch_bkey_fields fields_cnt)
{
    struct bkey_local_buffer buffer = {0};
    if (bkey->format == KEY_FORMAT_LOCAL_BTREE)
    {
        uint64_t *value = buffer.buffer;
        const uint8_t *bytes = (const void*)bkey;
        bytes += format->key_u64s * BCH_U64S_SIZE;
        for (enum bch_bkey_fields i = 0; i < fields_cnt; ++i, ++value)
        {
            *value = format->field_offset[i];
            if (*value + format->bits_per_field[i] == 0)
            {
                continue;
            }
            if (format->bits_per_field[i])
            {
                bytes -= format->bits_per_field[i] / 8;
                *value += benz_uintXX_as_uint64(bytes, format->bits_per_field[i]);
            }
        }
        buffer.key_u64s = format->key_u64s;
    }
    return buffer;
}

uint64_t benz_bch_parse_bkey_field(const struct bkey *bkey, const struct bkey_format *format, enum bch_bkey_fields field)
{
    if (bkey->format != KEY_FORMAT_LOCAL_BTREE)
    {
        switch ((int)field)
        {
        case BKEY_FIELD_INODE:
            return bkey->p.inode;
        case BKEY_FIELD_OFFSET:
            return bkey->p.offset;
        case BKEY_FIELD_SNAPSHOT:
            return bkey->p.snapshot;
        case BKEY_FIELD_SIZE:
            return bkey->size;
        case BKEY_FIELD_VERSION_HI:
            return bkey->version.hi;
        case BKEY_FIELD_VERSION_LO:
            return bkey->version.lo;
        }
    }

    uint64_t value = format->field_offset[field];
    if (format->bits_per_field[field])
    {
        const uint8_t *bytes = (const void*)bkey;
        bytes += format->key_u64s * BCH_U64S_SIZE;
        for (enum bch_bkey_fields i = 0; i <= field; ++i)
        {
            bytes -= format->bits_per_field[i] / 8;
        }
        return value + benz_uintXX_as_uint64(bytes, format->bits_per_field[field]);
    }
    else
    {
        return value;
    }
}

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

inline uint64_t benz_bch_get_block_size(const struct bch_sb *sb)
{
    return (uint64_t)sb->block_size * BCH_SECTOR_SIZE;
}

inline uint64_t benz_bch_get_btree_node_size(const struct bch_sb *sb)
{
    return (uint64_t)(uint16_t)benz_get_flag_bits(sb->flags[0], 12, 28) * BCH_SECTOR_SIZE;
}

inline uint64_t benz_bch_get_extent_offset(const struct bch_extent_ptr *bch_extent_ptr)
{
    return bch_extent_ptr->offset * BCH_SECTOR_SIZE;
}

const struct bkey *benz_bch_file_offset_size(const struct bkey *bkey,
                                             const struct bch_val *bch_val,
                                             uint64_t *file_offset,
                                             uint64_t *offset,
                                             uint64_t *size)
{
    if (bch_val && bkey->type == KEY_TYPE_extent)
    {
        *file_offset = (bkey->p.offset - bkey->size) * BCH_SECTOR_SIZE;
        *offset = benz_bch_get_extent_offset((const struct bch_extent_ptr*)bch_val);
        *size = bkey->size * BCH_SECTOR_SIZE;
    }
    else if (bch_val && bkey->type == KEY_TYPE_inline_data)
    {
        *file_offset = (bkey->p.offset - bkey->size) * BCH_SECTOR_SIZE;
        *offset = 0;
        *size = bkey->u64s * BCH_U64S_SIZE;
    }
    else
    {
        bkey = NULL;
    }
    return bkey;
}

uint64_t benz_bch_inline_data_offset(const struct btree_node* start, const struct bch_val *bch_val, uint64_t start_offset)
{
    return (uint64_t)((const uint8_t*)bch_val - (const uint8_t*)start) + start_offset;
}

// Get the superblock size, if sb is null return the minimal size it can be so
// we can extract the full size to allocate for. Once the superblock was
// allocated once we can extract is real size.
uint64_t benz_bch_get_sb_size(const struct bch_sb *sb)
{
    uint64_t size = 0;
    if (sb == NULL)
    {
        size = sizeof(struct bch_sb);
    }
    else if (memcmp(&sb->magic, &BCACHE_MAGIC, sizeof(BCACHE_MAGIC)) == 0)
    {
        size = sizeof(struct bch_sb) + sb->u64s * BCH_U64S_SIZE;
    }
    return size;
}

struct bch_sb *benz_bch_realloc_sb(struct bch_sb *sb, uint64_t size)
{
    if (size == 0)
    {
        size = benz_bch_get_sb_size(sb);
    }
    struct bch_sb *ret = realloc(sb, size);
    if (ret == NULL && sb)
    {
        free(sb);
    }
    return ret;
}

inline struct btree_node *benz_bch_malloc_btree_node(const struct bch_sb *sb)
{
    return malloc(benz_bch_get_btree_node_size(sb));
}

uint64_t benz_bch_fread_sb(struct bch_sb *sb, uint64_t size, FILE *fp)
{
    if (size == 0)
    {
        size = benz_bch_get_sb_size(NULL);
    }
    fseek(fp, BCH_SB_SECTOR * BCH_SECTOR_SIZE, SEEK_SET);
    return fread(sb, size, 1, fp);
}

uint64_t benz_bch_fread_btree_node(struct btree_node *btree_node, const struct bch_sb *sb, const struct bch_btree_ptr_v2 *btree_ptr, FILE *fp)
{
    uint64_t offset = benz_bch_get_extent_offset(btree_ptr->start);
    fseek(fp, (long)offset, SEEK_SET);
    memset(btree_node, 0, benz_bch_get_btree_node_size(sb));
    return fread(btree_node, btree_ptr->sectors_written * BCH_SECTOR_SIZE, 1, fp);
}

// Filesystem and iterator abstraction layer
// -----------------------------------------
int Bcachefs_open(Bcachefs *this, const char *path)
{
    *this = Bcachefs_clean;

    int ret = 0;
    this->fp = fopen(path, "rb");
    if (this->fp)
    {
        fseek(this->fp, 0L, SEEK_END);
        this->size = ftell(this->fp);
        fseek(this->fp, 0L, SEEK_SET);
        this->sb = benz_bch_realloc_sb(NULL, 0);
    }
    if (this->sb && benz_bch_fread_sb(this->sb, 0, this->fp))
    {
        this->sb = benz_bch_realloc_sb(this->sb, 0);
        ret = this->sb && benz_bch_fread_sb(this->sb, benz_bch_get_sb_size(this->sb),
                                            this->fp) &&
            Bcachefs_iter_reinit(this, &this->_extents_iter_begin, BTREE_ID_extents) &&
            Bcachefs_iter_reinit(this, &this->_inodes_iter_begin, BTREE_ID_inodes) &&
            Bcachefs_iter_reinit(this, &this->_dirents_iter_begin, BTREE_ID_dirents);
    }
    if (ret)
    {
        this->_root_stats = Bcachefs_find_inode(this, BCACHEFS_ROOT_INO);
        this->_root_dirent = (Bcachefs_dirent){.parent_inode=BCACHEFS_ROOT_INO,
                                               .inode=BCACHEFS_ROOT_INO,
                                               .type=4,
                                               .name=(const void*)"",
                                               .name_len=1};
        ret = this->_root_stats.hash_seed != 0;
    }
    if (!ret)
    {
        Bcachefs_close(this);
    }
    return ret;
}

int Bcachefs_close(Bcachefs *this)
{
    if (this->fp && !fclose(this->fp))
    {
        this->fp = NULL;
        this->size = 0;
    }
    if (this->sb)
    {
        free(this->sb);
        this->sb = NULL;
    }
    this->_root_stats = (Bcachefs_inode){0};
    this->_root_dirent = (Bcachefs_dirent){0};
    return this->fp == NULL && this->sb == NULL &&
        Bcachefs_iter_fini(this, &this->_extents_iter_begin) &&
        Bcachefs_iter_fini(this, &this->_inodes_iter_begin) &&
        Bcachefs_iter_fini(this, &this->_dirents_iter_begin);
}

Bcachefs_iterator* Bcachefs_iter(const Bcachefs *this, enum btree_id type)
{
    Bcachefs_iterator *iter = malloc(sizeof(Bcachefs_iterator));
    *iter = Bcachefs_iterator_clean;
    const Bcachefs_iterator *iter_begin = NULL;
    switch ((int)type)
    {
    case BTREE_ID_extents:
        iter_begin = &this->_extents_iter_begin;
        break;
    case BTREE_ID_inodes:
        iter_begin = &this->_inodes_iter_begin;
        break;
    case BTREE_ID_dirents:
        iter_begin = &this->_dirents_iter_begin;
        break;
    }
    if (iter_begin &&
        Bcachefs_iter_minimal_copy(this, iter, iter_begin)) {}
    else
    {
        free(iter);
        iter = NULL;
    }
    return iter;
}

int Bcachefs_next_iter(const Bcachefs *this, Bcachefs_iterator *iter, const struct bch_btree_ptr_v2 *btree_ptr)
{
    Bcachefs_iterator *next_it = malloc(sizeof(Bcachefs_iterator));
    *next_it = (Bcachefs_iterator){
        .type = iter->type,
        .btree_node = benz_bch_malloc_btree_node(this->sb),
        .btree_ptr = btree_ptr
    };

    if (Bcachefs_iter_reinit(this, next_it, BTREE_ID_NR))
    {
        iter->next_it = next_it;
        return 1;
    }
    else
    {
        Bcachefs_iter_fini(this, next_it);
        free(next_it);
        next_it = NULL;
        return 0;
    }
}

int _Bcachefs_find_bkey_lesser_than(struct bkey_local_buffer *buffer, struct bkey_local_buffer *reference)
{
    enum bch_bkey_fields field = 0;
    for (; field < BKEY_NR_FIELDS && buffer->buffer[field] == reference->buffer[field]; ++field) {}
    return field < BKEY_NR_FIELDS && buffer->buffer[field] < reference->buffer[field];
}

const struct bkey* _Bcachefs_find_bkey(const Bcachefs *this, Bcachefs_iterator *iter, struct bkey_local_buffer *reference)
{
    struct bkey_local_buffer lower_bkey_value = {0};
    struct bkey_local_buffer bkey_value = {0};
    for (iter->bset = Bcachefs_iter_next_bset(this, iter); iter->bset;
         iter->bset = Bcachefs_iter_next_bset(this, iter))
    {
        const struct bkey *lower_bkey = NULL;
        const struct bkey *bkey = NULL;
        do
        {
            lower_bkey = bkey;
            lower_bkey_value = bkey_value;
            bkey = benz_bch_next_bkey(iter->bset, bkey, KEY_TYPE_MAX);
            if (bkey)
            {
                switch ((int)iter->type)
                {
                case BTREE_ID_inodes:
                case BTREE_ID_dirents:
                    bkey_value = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_FIELD_OFFSET + 1);
                    break;
                }
            }
            else
            {
                bkey_value = (struct bkey_local_buffer){0};
            }
        } while (bkey && _Bcachefs_find_bkey_lesser_than(&bkey_value, reference));

        if (!memcmp(bkey_value.buffer, reference->buffer, sizeof(bkey_value.buffer)))
        {
            uint8_t key_u64s = bkey->format == KEY_FORMAT_LOCAL_BTREE ?
                iter->btree_node->format.key_u64s : BKEY_U64s;
            iter->bkey = bkey;
            iter->bch_val = benz_bch_first_bch_val(bkey, key_u64s);
            return bkey;
        }
        else if (lower_bkey && _Bcachefs_find_bkey_lesser_than(&lower_bkey_value, reference))
        {
            uint8_t key_u64s = lower_bkey->format == KEY_FORMAT_LOCAL_BTREE ?
                iter->btree_node->format.key_u64s : BKEY_U64s;
            iter->bkey = lower_bkey;
            iter->bch_val = benz_bch_first_bch_val(lower_bkey, key_u64s);
            switch (lower_bkey->type)
            {
            case KEY_TYPE_btree_ptr_v2:
                if (Bcachefs_next_iter(this, iter, (const struct bch_btree_ptr_v2*)iter->bch_val))
                {
                    bkey = _Bcachefs_find_bkey(this, iter, reference);
                    if (bkey)
                    {
                        return bkey;
                    }
                }
                break;
            }
        }
    }

    return NULL;
}

Bcachefs_inode Bcachefs_find_inode(const Bcachefs *this, uint64_t inode)
{
    if (inode == this->_root_stats.inode)
    {
        return this->_root_stats;
    }
    Bcachefs_inode stats = {0};
    Bcachefs_iterator *iter = Bcachefs_iter(this, BTREE_ID_inodes);
    struct bkey_local_buffer reference = {0};
    reference.buffer[BKEY_FIELD_OFFSET] = inode;
    const struct bkey *bkey = _Bcachefs_find_bkey(this, iter, &reference);
    if (bkey)
    {
        switch (bkey->type)
        {
        case KEY_TYPE_deleted:
            stats.inode = inode;
        default:
            stats = Bcachefs_iter_make_inode(this, iter);
        }
    }
    Bcachefs_iter_fini(this, iter);
    free(iter);
    return stats;
}

Bcachefs_dirent Bcachefs_find_dirent(const Bcachefs *this, uint64_t parent_inode, uint64_t hash_seed, const uint8_t *name, const uint8_t len)
{
    if (!strcmp((const void*)name, (const void*)this->_root_dirent.name))
    {
        return this->_root_dirent;
    }
    Bcachefs_dirent dirent = {0};
    if (!hash_seed)
    {
        hash_seed = Bcachefs_find_inode(this, parent_inode).hash_seed;
    }
    if (!hash_seed)
    {
        return dirent;
    }
    uint64_t offset = benz_siphash_digest(name, len, hash_seed, 0) >> 1;
    Bcachefs_iterator *iter = Bcachefs_iter(this, BTREE_ID_dirents);
    struct bkey_local_buffer reference = {0};
    reference.buffer[BKEY_FIELD_INODE] = parent_inode;
    reference.buffer[BKEY_FIELD_OFFSET] = offset;
    const struct bkey *bkey = _Bcachefs_find_bkey(this, iter, &reference);
    if (bkey)
    {
        switch (bkey->type)
        {
        case KEY_TYPE_deleted:
            dirent.parent_inode = parent_inode;
            dirent.name = name;
            dirent.name_len = len;
        default:
            dirent = Bcachefs_iter_make_dirent(this, iter);
        }
    }
    Bcachefs_iter_fini(this, iter);
    free(iter);
    return dirent;
}

void _Bcachefs_iter_build_bsets_cache(const Bcachefs *this, Bcachefs_iterator *iter)
{
    iter->_bsets = malloc(sizeof(struct bset*) * 8);
    iter->_bset = (void*)iter->_bsets;
    iter->_bsets_end = (void*)(iter->_bsets + 8);
    const struct bset *bset = NULL;
    const void *btree_node_end = (const uint8_t*)iter->btree_node + iter->btree_ptr->sectors_written * BCH_SECTOR_SIZE;
    const struct bset **swp = NULL;
    while ((bset = benz_bch_next_bset(iter->btree_node, btree_node_end, bset, this->sb)))
    {
        *(iter->_bset++) = bset;
        if (iter->_bset == iter->_bsets_end)
        {
            swp = (void*)iter->_bsets;
            const uint32_t num = iter->_bsets_end - (const struct bset**)iter->_bsets;
            iter->_bsets = malloc(sizeof(struct bset*) * num * 2);
            memcpy(iter->_bsets, swp, sizeof(struct bset*) * num);
            free(swp);
            swp = NULL;
        }
    }
    iter->_bsets_end = iter->_bset;
}

int Bcachefs_iter_reinit(const Bcachefs *this, Bcachefs_iterator *iter, enum btree_id type)
{
    if (!memcmp(iter, &Bcachefs_iterator_clean, sizeof(Bcachefs_iterator)))
    {
        // Initialize the iterator
        iter->type = type;
        iter->btree_node = benz_bch_malloc_btree_node(this->sb);
        iter->jset_entry = Bcachefs_iter_next_jset_entry(this, iter);
        iter->btree_ptr = Bcachefs_iter_next_btree_ptr(this, iter);
    }
    else
    {
        // Reinitialize the btree pointers using the existing btree
        Bcachefs_iter_fini(this, iter->next_it);
        iter->next_it = NULL;
        free(iter->_bsets);
        iter->_bsets = NULL;
        *iter = (Bcachefs_iterator){
            .type = iter->type,
            .btree_ptr = iter->btree_ptr,
            .btree_node = iter->btree_node
        };
    }
    if (iter->btree_ptr && !benz_bch_fread_btree_node(iter->btree_node,
                                                      this->sb,
                                                      iter->btree_ptr,
                                                      this->fp))
    {
        iter->btree_ptr = NULL;
    }
    if (iter->btree_ptr)
    {
        // Build a cache of the bsets in the btree to enable backward iteration
        _Bcachefs_iter_build_bsets_cache(this, iter);
    }
    return iter->jset_entry && iter->btree_node && iter->btree_ptr;
}

int Bcachefs_iter_minimal_copy(const Bcachefs *this, Bcachefs_iterator *iter, const Bcachefs_iterator *other)
{
    if (memcmp(iter, &Bcachefs_iterator_clean, sizeof(Bcachefs_iterator)))
    {
        return 0;
    }
    iter->type = other->type;
    iter->jset_entry = other->jset_entry;
    iter->btree_ptr = other->btree_ptr;
    if (other->next_it)
    {
        iter->next_it = malloc(sizeof(Bcachefs_iterator));
        Bcachefs_iter_minimal_copy(this, iter->next_it, other->next_it);
    }

    iter->btree_node = benz_bch_malloc_btree_node(this->sb);
    memcpy(iter->btree_node, other->btree_node, benz_bch_get_btree_node_size(this->sb));
    const uint32_t num = other->_bsets_end - (const struct bset**)other->_bsets;
    if (num)
    {
        iter->_bsets = malloc(sizeof(struct bset*) * num);
    }
    else
    {
        iter->_bsets = NULL;
    }
    iter->_bsets_end = (const struct bset**)(iter->_bsets + num);
    iter->_bset = (const struct bset**)iter->_bsets;
    for (int i = 0; iter->_bset < iter->_bsets_end; ++i, ++iter->_bset)
    {
        const uint64_t offset = (const uint8_t*)other->_bsets[i] - (const uint8_t*)other->btree_node;
        *iter->_bset = (const void*)((const uint8_t*)iter->btree_node + offset);
    }

    return 1;
}

int Bcachefs_iter_fini(const Bcachefs *this, Bcachefs_iterator *iter)
{
    (void)this;
    if (iter == NULL)
    {
        return 1;
    }
    if (iter->next_it && Bcachefs_iter_fini(this, iter->next_it))
    {
        free(iter->next_it);
        iter->next_it = NULL;
    }
    if (iter->btree_node)
    {
        free(iter->btree_node);
        iter->btree_node = NULL;
    }
    if (iter->_bsets)
    {
        free(iter->_bsets);
        iter->_bsets = NULL;
    }
    *iter = (Bcachefs_iterator){
        .type = BTREE_ID_NR,
        .btree_node = iter->btree_node,
        .next_it = iter->next_it,
        ._bsets = iter->_bsets
    };
    return iter->next_it == NULL && iter->btree_node == NULL &&
        iter->_bsets == NULL;
}

const struct bch_val *_Bcachefs_iter_next_bch_val(const struct bkey *bkey, const struct bkey_format *format)
{
    if (bkey == NULL)
    {
        return NULL;
    }
    uint8_t key_u64s = 0;
    if (bkey->format == KEY_FORMAT_LOCAL_BTREE)
    {
        key_u64s = format->key_u64s;
    }
    else
    {
        key_u64s = BKEY_U64s;
    }
    return benz_bch_first_bch_val(bkey, key_u64s);
}

const struct bch_val *Bcachefs_iter_next(const Bcachefs *this, Bcachefs_iterator *iter)
{
    const struct bkey *bkey = NULL;
    const struct bch_val *bch_val = NULL;

    // Wind to current iterator
    if (iter->next_it)
    {
        bch_val = Bcachefs_iter_next(this, iter->next_it);
        if (bch_val)
        {
            return bch_val;
        }
        else
        {
            Bcachefs_iter_fini(this, iter->next_it);
            free(iter->next_it);
            iter->next_it = NULL;
        }
    }
    if (iter->bkey) {}
    else
    {
        iter->bset = Bcachefs_iter_next_bset(this, iter);
    }
    if (iter->bset) {}
    else
    {
        return NULL;
    }
    do
    {
        iter->bkey = benz_bch_next_bkey(iter->bset, iter->bkey, KEY_TYPE_MAX);
        bch_val = _Bcachefs_iter_next_bch_val(iter->bkey, &iter->btree_node->format);
    } while (iter->bkey && bch_val == NULL);
    bkey = iter->bkey;
    switch ((int)iter->type)
    {
    case BTREE_ID_extents:
    case BTREE_ID_inodes:
    case BTREE_ID_dirents:
        iter->bch_val = bch_val;
        if (bch_val && bkey->type == KEY_TYPE_btree_ptr_v2 &&
                Bcachefs_next_iter(this, iter, (const struct bch_btree_ptr_v2*)bch_val))
        {
            return Bcachefs_iter_next(this, iter);
        }
        else if (bch_val)
        {
            return bch_val;
        }
        break;
    default:
        return NULL;
    }
    return Bcachefs_iter_next(this, iter);
}

const struct jset_entry *Bcachefs_iter_next_jset_entry(const Bcachefs *this, Bcachefs_iterator *iter)
{
    const struct jset_entry *jset_entry = iter->jset_entry;
    const struct bch_sb_field *sb_field_clean = (const void*)benz_bch_next_sb_field(
                this->sb,
                NULL,
                BCH_SB_FIELD_clean);

    // if sb_field_clean == NULL then the archive needs to be fsck
    // TODO: we need to return an error all the way back to python here
    assert(sb_field_clean != NULL);
    jset_entry = benz_bch_next_jset_entry(sb_field_clean,
                                          sizeof(struct bch_sb_field_clean),
                                          jset_entry,
                                          BCH_JSET_ENTRY_btree_root);

    assert(jset_entry != NULL);
    for (; jset_entry && jset_entry->btree_id != iter->type;
         jset_entry = benz_bch_next_jset_entry(sb_field_clean,
                                               sizeof(struct bch_sb_field_clean),
                                               jset_entry,
                                               BCH_JSET_ENTRY_btree_root)) {}
    return jset_entry;
}

const struct bch_btree_ptr_v2 *Bcachefs_iter_next_btree_ptr(const Bcachefs *this, Bcachefs_iterator *iter)
{
    (void)this;
    const struct jset_entry *jset_entry = iter->jset_entry;
    const struct bch_btree_ptr_v2 *btree_ptr = iter->btree_ptr;
    if (btree_ptr)
    {
        btree_ptr = (const void*)benz_bch_next_bch_val(&jset_entry->start->k,
                                                       (const void*)btree_ptr,
                                                       sizeof(struct bch_btree_ptr_v2));
    }
    else
    {
        btree_ptr = (const void*)benz_bch_first_bch_val(&jset_entry->start->k, BKEY_U64s);
    }
    const struct bch_val *bch_val = (const void*)btree_ptr;
    for (; bch_val && btree_ptr->start->unused;
         bch_val = benz_bch_next_bch_val(&jset_entry->start->k,
                                         bch_val,
                                         sizeof(struct bch_btree_ptr_v2)),
         btree_ptr = (const void*)bch_val) {}
    return btree_ptr;
}

const struct bset *Bcachefs_iter_next_bset(const Bcachefs *this, Bcachefs_iterator *iter)
{
    // Reverse iteration of bsets as it is assumed that the last bset contains
    // the most up to date bkeys
    --iter->_bset;
    if (iter->_bset < (const struct bset**)iter->_bsets)
    {
        iter->_bset = iter->_bsets_end;
        return NULL;
    }
    return *iter->_bset;
}

Bcachefs_extent Bcachefs_iter_make_extent(const Bcachefs *this, Bcachefs_iterator *iter)
{
    (void)this;

    while (iter->next_it)
    {
        iter = iter->next_it;
    }

    const struct bkey_local_buffer buffer = benz_bch_parse_bkey_buffer(iter->bkey, &iter->btree_node->format, BKEY_FIELD_OFFSET + 1);
    const struct bkey_local bkey_local = benz_bch_parse_bkey(iter->bkey, &buffer);
    const struct bkey *bkey = (const void*)&bkey_local;
    Bcachefs_extent extent = {.inode = bkey->p.inode};
    benz_bch_file_offset_size(bkey, iter->bch_val, &extent.file_offset, &extent.offset, &extent.size);
    if (bkey->type == KEY_TYPE_inline_data)
    {
        extent.offset = benz_bch_inline_data_offset(iter->btree_node, iter->bch_val,
                                                    benz_bch_get_extent_offset(iter->btree_ptr->start));
        extent.size -= (uint64_t)((const uint8_t*)iter->bch_val - (const uint8_t*)iter->bkey);
    }
    return extent;
}

Bcachefs_inode Bcachefs_iter_make_inode(const Bcachefs *this, Bcachefs_iterator *iter)
{
    (void)this;

    while (iter->next_it)
    {
        iter = iter->next_it;
    }

    const struct bkey *bkey = iter->bkey;
    const struct bkey_local_buffer buffer = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_FIELD_OFFSET + 1);
    const struct bkey_local bkey_local = benz_bch_parse_bkey(bkey, &buffer);
    const struct bch_inode *bch_inode = (const void*)iter->bch_val;

    const void *p_end = (const void*)((const uint8_t*)bkey + bkey->u64s * BCH_U64S_SIZE);

    Bcachefs_inode inode = {.inode = bkey_local.p.offset,
                            .hash_seed = bch_inode->bi_hash_seed};

    benz_bch_inode_unpack_size(&inode.size, bch_inode, p_end);
    return inode;
}

Bcachefs_dirent Bcachefs_iter_make_dirent(const Bcachefs *this, Bcachefs_iterator *iter)
{
    (void)this;

    while (iter->next_it)
    {
        iter = iter->next_it;
    }
    const struct bkey *bkey = iter->bkey;
    const struct bkey_local_buffer buffer = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_FIELD_OFFSET + 1);
    const struct bkey_local bkey_local = benz_bch_parse_bkey(bkey, &buffer);
    const struct bch_dirent *bch_dirent = (const void*)iter->bch_val;
    const uint8_t name_len = strlen((const void*)bch_dirent->d_name);
    const uint8_t max_name_len = (const uint8_t*)bkey + bkey->u64s * BCH_U64S_SIZE - bch_dirent->d_name;
    return (Bcachefs_dirent){.parent_inode = bkey_local.p.inode,
                                  .inode = bch_dirent->d_inum,
                                  .type = bch_dirent->d_type,
                                  .name = bch_dirent->d_name,
                                  .name_len = (name_len < max_name_len ? name_len : max_name_len)};
}

inline uint64_t benz_get_flag_bits(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit)
{
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}

uint64_t benz_uintXX_as_uint64(const uint8_t *bytes, uint8_t sizeof_uint)
{
    switch (sizeof_uint)
    {
    case 64:
        return *(const uint64_t*)(const void*)bytes;
    case 32:
        return *(const uint32_t*)(const void*)bytes;
    case 16:
        return *(const uint16_t*)(const void*)bytes;
    case 8:
        return *(const uint8_t*)bytes;
    }
    return (uint64_t)-1;
}

void benz_print_chars(const uint8_t* bytes, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        printf("%c", bytes[i]);
    }
}

void benz_print_bytes(const uint8_t* bytes, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        if (i && i % 4 == 0)
        {
            printf(" ");
        }
        if (i && i % 32 == 0)
        {
            printf("\n");
        }
        benz_print_hex(bytes + i, 1);
    }
}

void benz_print_bits(uint64_t bitfield)
{
    uint8_t* bytes = (uint8_t*)&bitfield;
    for (int i = 0, e = sizeof(bitfield) / sizeof(uint8_t); i < e; ++i)
    {
        for (int j = sizeof(uint8_t) * 8; j > 0; --j)
        {
            if (bytes[i] & 128)
            {
                printf("1");
            }
            else
            {
                printf("0");
            }
            bytes[i] <<= 1;
        }
        printf(" ");
    }
}

void benz_print_hex(const uint8_t *hex, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        printf("%02x", hex[i]);
    }
}

void benz_print_uuid(const struct uuid *uuid)
{
    unsigned int i = 0;
    benz_print_hex(&uuid->bytes[i], 4);
    i+=4;
    printf("-");
    benz_print_hex(&uuid->bytes[i], 2);
    i+=2;
    printf("-");
    benz_print_hex(&uuid->bytes[i], 2);
    i+=2;
    printf("-");
    benz_print_hex(&uuid->bytes[i], 2);
    i+=2;
    printf("-");
    benz_print_hex(&uuid->bytes[i], sizeof(*uuid) - i);
}
