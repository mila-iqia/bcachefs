#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bcachefs.h"

const void *benz_bch_next_sibling(const void *p, uint32_t sizeof_p, const void *p_end, const void *c, struct u64s_spec u64s_spec)
{
    if (c == NULL)
    {
        c = (const uint8_t*)p + sizeof_p;
    }
    else
    {
        uint64_t u64s = 0;
        switch (u64s_spec.size) {
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
        u64s += u64s_spec.start;
        c = (const uint8_t*)c + u64s * BCH_U64S_SIZE;
    }
    if (c >= p_end)
    {
        c = NULL;
    }
    return c;
}

const struct bch_sb_field *benz_bch_next_sb_field(const struct bch_sb *p, const struct bch_sb_field *c, enum bch_sb_field_type type)
{
    const uint8_t *p_end = (const uint8_t*)p + p->u64s * BCH_U64S_SIZE;
    do
    {
        c = (const struct bch_sb_field*)benz_bch_next_sibling(p, sizeof(*p), p_end, c, U64S_BCH_SB_FIELD);
    } while (c && type != BCH_SB_FIELD_NR && c->type != type);
    return c;
}

const struct jset_entry *benz_bch_next_jset_entry(const struct bch_sb_field *p,
                                                  uint32_t sizeof_p,
                                                  const struct jset_entry *c,
                                                  enum bch_jset_entry_type type)
{
    const uint8_t *p_end = (const uint8_t*)p + p->u64s * BCH_U64S_SIZE;
    do
    {
        c = (const struct jset_entry*)benz_bch_next_sibling(p, sizeof_p, p_end, c, U64S_JSET_ENTRY);
    } while (c && type != BCH_SB_FIELD_NR && c->type != type);
    return c;
}

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

const struct bset *benz_bch_next_bset(const struct btree_node *p, const struct bset *c, const struct bch_sb *sb)
{
    uint64_t btree_node_size = benz_bch_get_btree_node_size(sb);
    uint64_t block_size = benz_bch_get_block_size(sb);
    const struct bset *p_end = (const void*)((const uint8_t*)p + btree_node_size);
    do
    {
        if (c == NULL)
        {
            c = &p->keys;
        }
        else
        {
            const uint8_t *_cb = (const uint8_t*)c;
            _cb -= (uint64_t)p;
            _cb += sizeof(*c) + c->u64s * BCH_U64S_SIZE;
            _cb += block_size - (uint64_t)_cb % block_size +
                   // skip btree_node_entry csum
                   sizeof(struct bch_csum);
            _cb += (uint64_t)p;
            c = (const void*)_cb;
        }
        if (c >= p_end)
        {
            c = NULL;
        }
    } while (c && !c->u64s);
    return c;
}

const struct bkey *benz_bch_next_bkey(const struct bset *p, const struct bkey *c, enum bch_bkey_type type)
{
    const uint8_t *p_end = (const uint8_t*)p + p->u64s * BCH_U64S_SIZE;
    do
    {
        c = (const struct bkey*)benz_bch_next_sibling(p, sizeof(*p), p_end, c, U64S_BKEY);
    } while (c && type != KEY_TYPE_MAX && c->type != type);
    return c;
}

struct bkey_local benz_bch_parse_bkey(const struct bkey *bkey, const struct bkey_format *format)
{
    struct bkey_local ret = {.u64s = bkey->u64s,
                             .format = bkey->format,
                             .needs_whiteout = bkey->needs_whiteout,
                             .type = bkey->type};
    if (bkey->format == KEY_FORMAT_LOCAL_BTREE &&
            memcmp(format, &BKEY_FORMAT_SHORT, sizeof(struct bkey_format)) == 0)
    {
        const struct bkey_short *bkey_short = (const void*)bkey;
        ret.p = bkey_short->p;
        ret.key_u64s = format->key_u64s;
    }
    else if (bkey->format == KEY_FORMAT_LOCAL_BTREE &&
             // Does not support field_offset yet
             memcmp(format->field_offset, (uint64_t[6]){0}, sizeof(format->field_offset)) == 0)
    {
        const uint8_t *bytes = (const void*)bkey;
        bytes += format->key_u64s * BCH_U64S_SIZE;
        for (int i = 0; i < BKEY_NR_FIELDS ; ++i)
        {
            if (format->bits_per_field[i] == 0)
            {
                continue;
            }
            bytes -= format->bits_per_field[i] / 8;
            uint64_t value = benz_uintXX_as_uint64(bytes, format->bits_per_field[i]);
            switch (i) {
            case BKEY_FIELD_INODE:
                ret.p.inode = value;
                break;
            case BKEY_FIELD_OFFSET:
                ret.p.offset = value;
                break;
            case BKEY_FIELD_SNAPSHOT:
                ret.p.snapshot = (uint32_t)value;
                break;
            case BKEY_FIELD_SIZE:
                ret.size = (uint32_t)value;
                break;
            case BKEY_FIELD_VERSION_HI:
                ret.version.hi = (uint32_t)value;
                break;
            case BKEY_FIELD_VERSION_LO:
                ret.version.lo = value;
                break;
            }
        }
        ret.key_u64s = format->key_u64s;
    }
    else if (bkey->format == KEY_FORMAT_CURRENT)
    {
        memcpy(&ret, bkey, sizeof(*bkey));
        ret.key_u64s = BKEY_U64s;
    }
    return ret;
}

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

uint64_t benz_bch_fread_btree_node(struct btree_node *btree_node, const struct bch_sb *sb, const struct bch_extent_ptr *bch_extent_ptr, FILE *fp)
{
    uint64_t offset = benz_bch_get_extent_offset(bch_extent_ptr);
    fseek(fp, (long)offset, SEEK_SET);
    return fread(btree_node, benz_bch_get_btree_node_size(sb), 1, fp);
}

int BCacheFS_fini(BCacheFS *this)
{
    return BCacheFS_close(this);
}

int BCacheFS_open(BCacheFS *this, const char *path)
{
    if (!BCacheFS_close(this))
    {
        return 0;
    }
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
                                            this->fp);
    }
    if (!ret)
    {
        BCacheFS_fini(this);
    }
    return ret;
}

int BCacheFS_close(BCacheFS *this)
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
    return this->fp == NULL && this->sb == NULL;
}

int BCacheFS_iter(const BCacheFS *this, BCacheFS_iterator *iter, enum btree_id type)
{
    iter->type = type;
    iter->btree_node = benz_bch_malloc_btree_node(this->sb);
    iter->jset_entry = BCacheFS_iter_next_jset_entry(this, iter);
    iter->btree_ptr = BCacheFS_iter_next_btree_ptr(this, iter);
    if (iter->btree_ptr && !benz_bch_fread_btree_node(iter->btree_node,
                                                      this->sb,
                                                      iter->btree_ptr->start,
                                                      this->fp))
    {
        iter->btree_ptr = NULL;
    }
    return iter->jset_entry && iter->btree_node && iter->btree_ptr;
}

int BCacheFS_next_iter(const BCacheFS *this, BCacheFS_iterator *iter, const struct bch_btree_ptr_v2 *btree_ptr)
{
    BCacheFS_iterator *next_it = malloc(sizeof(BCacheFS_iterator));
    *next_it = (BCacheFS_iterator){
        .type = iter->type,
        .btree_node = benz_bch_malloc_btree_node(this->sb),
        .btree_ptr = btree_ptr
    };
    if (next_it->btree_ptr && !benz_bch_fread_btree_node(next_it->btree_node,
                                                         this->sb,
                                                         next_it->btree_ptr->start,
                                                         this->fp))
    {
        next_it->btree_ptr = NULL;
    }
    if (next_it->btree_node && next_it->btree_ptr)
    {
        iter->next_it = next_it;
        return 1;
    }
    else
    {
        BCacheFS_iter_fini(this, next_it);
        free(next_it);
        next_it = NULL;
        return 0;
    }
}

int BCacheFS_iter_fini(const BCacheFS *this, BCacheFS_iterator *iter)
{
    (void)this;
    if (iter == NULL)
    {
        return 1;
    }
    if (iter->next_it && BCacheFS_iter_fini(this, iter->next_it))
    {
        free(iter->next_it);
        iter->next_it = NULL;
    }
    if (iter->btree_node)
    {
        free(iter->btree_node);
        iter->btree_node = NULL;
    }
    *iter = (BCacheFS_iterator){
        .type = BTREE_ID_NR,
        .btree_node = iter->btree_node,
        .next_it = iter->next_it
    };
    return iter->next_it == NULL && iter->btree_node == NULL;
}

const struct bch_val *_BCacheFS_iter_next_bch_val(const struct bkey *bkey, const struct bkey_format* format)
{
    uint8_t key_u64s = 0;
    if (bkey == NULL)
    {
        return NULL;
    }
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

const struct bch_val *BCacheFS_iter_next(const BCacheFS *this, BCacheFS_iterator *iter)
{
    const struct bkey *bkey = NULL;
    const struct bch_val *bch_val = NULL;
    // Wind to current iterator
    if (iter->next_it)
    {
        bch_val = BCacheFS_iter_next(this, iter->next_it);
        if (bch_val)
        {
            return bch_val;
        }
        else
        {
            BCacheFS_iter_fini(this, iter->next_it);
            free(iter->next_it);
            iter->next_it = NULL;
        }
    }
    if (iter->bset == NULL && iter->btree_ptr)
    {
        iter->bset = BCacheFS_iter_next_bset(this, iter);
    }
    if (iter->btree_ptr && iter->bset) {}
    else
    {
        return NULL;
    }
    do
    {
        iter->bkey = benz_bch_next_bkey(iter->bset, iter->bkey, KEY_TYPE_MAX);
        bch_val = _BCacheFS_iter_next_bch_val(iter->bkey, &iter->btree_node->format);
    } while (iter->bkey && bch_val == NULL);
    bkey = iter->bkey;
    switch ((int)iter->type)
    {
    case BTREE_ID_extents:
        iter->bch_val = bch_val;
        if (bch_val && bkey->type == KEY_TYPE_btree_ptr_v2 &&
                BCacheFS_next_iter(this, iter, (const struct bch_btree_ptr_v2*)bch_val))
        {
            return BCacheFS_iter_next(this, iter);
        }
        else if (bch_val)
        {
            return bch_val;
        }
        break;
    case BTREE_ID_dirents:
        iter->bch_val = bch_val;
        if (bch_val && bkey->type == KEY_TYPE_btree_ptr_v2 &&
                BCacheFS_next_iter(this, iter, (const struct bch_btree_ptr_v2*)bch_val))
        {
            return BCacheFS_iter_next(this, iter);
        }
        else if (bch_val)
        {
            return bch_val;
        }
        break;
    default:
        return NULL;
    }
    if (iter->bkey == NULL)
    {
        iter->bset = BCacheFS_iter_next_bset(this, iter);
    }
    if (iter->bset == NULL)
    {
        iter->btree_ptr = NULL;
    }
    return BCacheFS_iter_next(this, iter);
}

const struct jset_entry *BCacheFS_iter_next_jset_entry(const BCacheFS *this, BCacheFS_iterator *iter)
{
    const struct jset_entry *jset_entry = iter->jset_entry;
    const struct bch_sb_field *sb_field_clean = (const void*)benz_bch_next_sb_field(
                this->sb,
                NULL,
                BCH_SB_FIELD_clean);
    jset_entry = benz_bch_next_jset_entry(sb_field_clean,
                                          sizeof(struct bch_sb_field_clean),
                                          jset_entry,
                                          BCH_JSET_ENTRY_btree_root);
    for (; jset_entry && jset_entry->btree_id != iter->type;
         jset_entry = benz_bch_next_jset_entry(sb_field_clean,
                                               sizeof(struct bch_sb_field_clean),
                                               jset_entry,
                                               BCH_JSET_ENTRY_btree_root)) {}
    return jset_entry;
}

const struct bch_btree_ptr_v2 *BCacheFS_iter_next_btree_ptr(const BCacheFS *this, BCacheFS_iterator *iter)
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

const struct bset *BCacheFS_iter_next_bset(const BCacheFS *this, BCacheFS_iterator *iter)
{
    struct btree_node *btree_node = iter->btree_node;
    const struct bset *bset = iter->bset;
    return benz_bch_next_bset(btree_node, bset, this->sb);
}

BCacheFS_extent BCacheFS_iter_make_extent(const BCacheFS *this, BCacheFS_iterator *iter)
{
    (void)this;
    while (iter->next_it)
    {
        iter = iter->next_it;
    }
    const struct bkey_local bkey_local = benz_bch_parse_bkey(iter->bkey, &iter->btree_node->format);
    const struct bkey *bkey = (const void*)&bkey_local;
    BCacheFS_extent extent = {.inode = bkey->p.inode};
    benz_bch_file_offset_size(bkey, iter->bch_val, &extent.file_offset, &extent.offset, &extent.size);
    if (bkey->type == KEY_TYPE_inline_data)
    {
        extent.offset = benz_bch_inline_data_offset(iter->btree_node, iter->bch_val,
                                                    benz_bch_get_extent_offset(iter->btree_ptr->start));
        extent.size -= (uint64_t)((const uint8_t*)iter->bch_val - (const uint8_t*)iter->bkey);
    }
    return extent;
}

BCacheFS_dirent BCacheFS_iter_make_dirent(const BCacheFS *this, BCacheFS_iterator *iter)
{
    (void)this;
    while (iter->next_it)
    {
        iter = iter->next_it;
    }
    const struct bkey_local bkey_local = benz_bch_parse_bkey(iter->bkey, &iter->btree_node->format);
    const struct bch_dirent *bch_dirent = (const void*)iter->bch_val;
    return (BCacheFS_dirent){.parent_inode = bkey_local.p.inode,
                                  .inode = bch_dirent->d_inum,
                                  .type = bch_dirent->d_type,
                                  .name = bch_dirent->d_name};
}

inline uint64_t benz_get_flag_bits(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit)
{
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}

uint64_t benz_uintXX_as_uint64(const uint8_t *bytes, uint8_t sizeof_uint)
{
    switch (sizeof_uint) {
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
    for (uint64_t i = 0; i < len; ++i) {
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

void benz_print_hex(const uint8_t *hex, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        printf("%02x", hex[i]);
    }
}

void benz_print_uuid(const struct uuid *uuid) {
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
