#include <stdlib.h>
#include <string.h>

#include "bcachefs.h"


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
            _cb += block_size - (uint64_t)_cb % block_size;

            _cb += (uint64_t)p;

            // Checksum is not supported yet so we expect it to be 0
            if (!memcmp(_cb, &(struct bch_csum){0}, sizeof(struct bch_csum)))
            {
                // skip btree_node_entry csum
                c = (const void*)(_cb + sizeof(struct bch_csum));
            }
            else
            {
                c = NULL;
            }
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
    struct bkey_local_buffer buffer = {{0}};
    uint64_t *value = buffer.buffer;
    if (bkey->format == KEY_FORMAT_LOCAL_BTREE)
    {
        const uint8_t *bytes = (const void*)bkey;
        bytes += format->key_u64s * BCH_U64S_SIZE;
        for (enum bch_bkey_fields i = 0; i < fields_cnt; ++i, ++value)
        {
            *value = format->field_offset[i];
            if (format->bits_per_field[i])
            {
                bytes -= format->bits_per_field[i] / 8;
                *value += benz_uintXX_as_uint64(bytes, format->bits_per_field[i]);
            }
        }
        buffer.key_u64s = format->key_u64s;
    }
    else if (bkey->format == KEY_FORMAT_CURRENT)
    {
        for (enum bch_bkey_fields i = 0; i < fields_cnt; ++i, ++value)
        {
            switch ((int)i)
            {
            case BKEY_FIELD_INODE:
                *value = bkey->p.inode;
                break;
            case BKEY_FIELD_OFFSET:
                *value = bkey->p.offset;
                break;
            case BKEY_FIELD_SNAPSHOT:
                *value = (uint64_t)bkey->p.snapshot;
                break;
            case BKEY_FIELD_SIZE:
                *value = (uint64_t)bkey->size;
                break;
            case BKEY_FIELD_VERSION_HI:
                *value = (uint64_t)bkey->version.hi;
                break;
            case BKEY_FIELD_VERSION_LO:
                *value = bkey->version.lo;
                break;
            }
        }
        buffer.key_u64s = BKEY_U64s;
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
    if (ret == NULL)
    {
        free(sb);
        sb = NULL;
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
