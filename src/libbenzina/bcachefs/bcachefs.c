#include <assert.h>
#include <stdio.h>
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

const struct bch_val *benz_bch_next_bch_val(const struct bkey *p, const struct bch_val *c, uint32_t sizeof_c)
{
    const struct bch_val *p_end = (const void*)((const uint8_t*)p + p->u64s * BCH_U64S_SIZE);
    if (c == NULL)
    {
        c = (const void*)((const uint8_t*)p + sizeof(*p));
    }
    else
    {
        c = (const void*)((const uint8_t*)c + sizeof_c);
    }
    if (c >= p_end)
    {
        c = NULL;
    }
    return c;
}

inline const struct btree_node *benz_bch_btree_node(const void *ptr, const struct bch_extent_ptr *bch_extent_ptr)
{
    return (const void*)((const uint8_t*)ptr + benz_bch_get_extent_offset(bch_extent_ptr));
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

const struct bkey_dirent *benz_bch_next_bkey_dirent(const struct bset *p, const struct bkey_dirent *c, uint64_t p_inode)
{
    do
    {
        c = (const void*)benz_bch_next_bkey(p, (const void*)c, KEY_TYPE_dirent);
    } while (c && p_inode != 0 && c->p.inode != p_inode);
    return c;
}

const struct bkey_dirent *benz_bch_parent_bkey_dirent(const struct bset *bset, const struct bkey_dirent *bkey_dirent)
{
    uint64_t parent_inode = bkey_dirent->p.inode;
    bkey_dirent = (const void*)benz_bch_next_bkey(bset, NULL, KEY_TYPE_dirent);
    const struct bch_dirent *bch_dirent = NULL;
    while (bkey_dirent)
    {
        bch_dirent = benz_bch_dirent(bkey_dirent);
        if (bch_dirent->d_inum == parent_inode)
        {
            break;
        }
        bkey_dirent = benz_bch_next_bkey_dirent(bset, bkey_dirent, 0);
    }
    return bkey_dirent;
}

const struct bch_dirent *benz_bch_dirent(const struct bkey_dirent *p)
{
    return (const void*)((const uint8_t*)p + sizeof(*p));
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

const struct bch_dirent *benz_bch_find_dirent_by_name(const struct bset *bset, uint64_t p_inode, const char* name)
{
    const struct bkey_dirent *bkey_dirent = benz_bch_next_bkey_dirent(bset, NULL, p_inode);
    const struct bch_dirent *bch_dirent = NULL;
    while (bkey_dirent)
    {
        bch_dirent = benz_bch_dirent(bkey_dirent);
        if (strcmp((const char*)bch_dirent->d_name, name) == 0)
        {
            break;
        }
        bkey_dirent = benz_bch_next_bkey_dirent(bset, bkey_dirent, p_inode);
        bch_dirent = NULL;
    }
    return bch_dirent;
}

uint64_t benz_bch_find_inode(const struct bset *bset, const char* path)
{
    uint64_t inode = 0;
    const struct bch_dirent *bch_dirent = NULL;
    char buffer[256] = {0};
    strcpy(buffer, path);
    size_t buffer_len = strlen(buffer);
    size_t d_name_start = 0;
    if (strcmp(path, "/") == 0)
    {
        return BCACHEFS_ROOT_INO;
    }
    for (int i = (int)buffer_len - 1; i >= 0; --i)
    {
        if (path[i] == '/')
        {
            buffer[i] = '\0';
        }
    }
    do
    {
        d_name_start += (path[d_name_start] == '/' ? 1 : 0);
        bch_dirent = benz_bch_find_dirent_by_name(bset, inode ? inode : BCACHEFS_ROOT_INO, buffer + d_name_start);
        if (bch_dirent)
        {
            inode = bch_dirent->d_inum;
        }
        d_name_start += strlen(buffer + d_name_start);
    } while (bch_dirent && d_name_start < buffer_len);
    if (bch_dirent == NULL)
    {
        inode = 0;
    }
    return inode;
}

char *benz_bch_strcpy_file_full_path(char *buffer_end, const struct bset *bset, const struct bkey_dirent *bkey_dirent)
{
    const struct bch_dirent* bch_dirent = NULL;
    --buffer_end;
    do
    {
        char last_path_first = buffer_end[0];
        bch_dirent = benz_bch_dirent(bkey_dirent);
        uint64_t name_len = strlen((const char*)bch_dirent->d_name);
        buffer_end -= name_len;
        strcpy(buffer_end, (const char*)bch_dirent->d_name);
        (buffer_end + name_len)[0] = last_path_first;
        --buffer_end;
        buffer_end[0] = '/';
        bkey_dirent = benz_bch_parent_bkey_dirent(bset, bkey_dirent);
    } while (bkey_dirent);
    return buffer_end;
}

const struct bkey * benz_bch_file_offset_size(const struct bkey *bkey, uint64_t *file_offset, uint64_t *offset, uint64_t *size)
{
    const struct bch_val *bch_val = benz_bch_next_bch_val(bkey, NULL, 0);
    if (bch_val && bkey->type == KEY_TYPE_extent)
    {
        *file_offset = (bkey->p.offset - bkey->size) * BCH_SECTOR_SIZE;
        *offset = benz_bch_get_extent_offset((const struct bch_extent_ptr*)bch_val);
        *size = bkey->size * BCH_SECTOR_SIZE;
    }
    else if (bch_val && bkey->type == KEY_TYPE_inline_data)
    {
        *file_offset = 0;
        *offset = 0;
        *size = bkey->u64s * BCH_U64S_SIZE - (uint64_t)((const uint8_t*)bch_val - (const uint8_t*)bkey);
    }
    else
    {
        bkey = NULL;
    }
    return bkey;
}

const struct bkey *benz_bch_next_file_offset_size(const struct bset *p,
                                                  const struct bkey *c,
                                                  uint64_t inode,
                                                  uint64_t *file_offset,
                                                  uint64_t *offset,
                                                  uint64_t *size)
{
    if (c == NULL || c->p.inode != inode)
    {
        do
        {
            c = benz_bch_next_bkey(p, c, KEY_TYPE_MAX);
        } while (c && c->p.inode != inode);
    }
    else if (c && c->p.inode == inode)
    {
        c = benz_bch_next_bkey(p, c, KEY_TYPE_MAX);
        if (c && c->p.inode != inode)
        {
            c = NULL;
        }
    }
    if (c)
    {
        c = benz_bch_file_offset_size(c, file_offset, offset, size);
    }
    return c;
}

uint64_t benz_bch_inline_data_offset(const struct btree_node* start, const struct bkey *bkey, uint64_t start_offset)
{
    uint64_t offset = 0;
    if (bkey && bkey->type == KEY_TYPE_inline_data)
    {
        const struct bch_val *bch_val = benz_bch_next_bch_val(bkey, NULL, 0);
        offset = (uint64_t)((const uint8_t*)bch_val - (const uint8_t*)start) + start_offset;
    }
    return offset;
}

struct bch_sb *benz_bch_realloc_sb(struct bch_sb *sb, uint64_t size)
{
    if (size == 0)
    {
        size = benz_bch_get_sb_size(sb);
    }
    return realloc(sb, size);
}

inline struct btree_node *benz_bch_malloc_btree_node(const struct bch_sb *sb)
{
    return malloc(benz_bch_get_btree_node_size(sb));
}

uint8_t *benz_bch_malloc_file(const struct bset* bset, uint64_t inode)
{
    uint64_t size = 0, frag_size = 0;
    uint64_t _ = 0;
    const struct bkey *bkey = benz_bch_next_file_offset_size(bset, NULL, inode, &_, &_, &frag_size);
    while (bkey)
    {
        size += frag_size;
        bkey = benz_bch_next_file_offset_size(bset, bkey, inode, &_, &_, &frag_size);
    }
    return (uint8_t*)malloc(size);
}

uint64_t benz_bch_fread_sb(struct bch_sb *sb, uint64_t size, FILE *fp)
{
    if (size == 0)
    {
        size = benz_bch_get_sb_size(sb);
    }
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

uint64_t benz_bch_fread_file(uint8_t *ptr, const struct bset* bset, uint64_t inode, FILE *fp)
{
    uint64_t file_size = 0, file_offset = 0, frag_offset = 0, frag_size = 0;
    const struct bkey *bkey = benz_bch_next_file_offset_size(bset, NULL, inode, &file_offset, &frag_offset, &frag_size);
    while (bkey)
    {
        printf("i:%llu, fo:%llu, o:%llu, s:%llu\n", inode, file_offset, frag_offset, frag_size);
        if (bkey->type == KEY_TYPE_inline_data)
        {
            const struct bch_val *bch_val = benz_bch_next_bch_val(bkey, NULL, 0);
            memcpy(ptr + file_size, bch_val, frag_size);
            file_size += frag_size;
        }
        else
        {
            fseek(fp, (long)frag_offset, SEEK_SET);
            if (fread(ptr + file_offset, frag_size, 1, fp))
            {
                file_size += frag_size;
            }
        }
        bkey = benz_bch_next_file_offset_size(bset, bkey, inode, &file_offset, &frag_offset, &frag_size);
    }
    return file_size;
}

inline uint64_t benz_get_flag_bits(const uint64_t bitfield, uint8_t first_bit, uint8_t last_bit)
{
    return bitfield << (sizeof(bitfield) * 8 - last_bit) >> (sizeof(bitfield) * 8 - last_bit + first_bit);
}

void print_chars(const uint8_t* bytes, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i)
    {
        printf("%c", bytes[i]);
    }
}

void print_bytes(const uint8_t* bytes, uint64_t len)
{
    for (uint64_t i = 0; i < len; ++i) {
        print_hex(bytes + i, 1);
        printf(" ");
        if ((i + 1) % 8 == 0)
        {
            printf(" ");
        }
        if ((i + 1) % 32 == 0)
        {
            printf("\n");
        }
    }
}

void print_bits(uint64_t bitfield)
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

void print_hex(const uint8_t *hex, uint64_t len) {
    for (uint64_t i = 0; i < len; ++i) {
        printf("%02x", hex[i]);
    }
}

void print_uuid(const struct uuid *uuid) {
    unsigned int i = 0;
    print_hex(&uuid->bytes[i], 4);
    i+=4;
    printf("-");
    print_hex(&uuid->bytes[i], 2);
    i+=2;
    printf("-");
    print_hex(&uuid->bytes[i], 2);
    i+=2;
    printf("-");
    print_hex(&uuid->bytes[i], 2);
    i+=2;
    printf("-");
    print_hex(&uuid->bytes[i], sizeof(*uuid) - i);
}

int BCacheFS_fini(BCacheFS *this)
{
    printf("BCacheFS_fini\n");
    int ret = BCacheFS_close(this);
    if (this->sb)
    {
        free(this->sb);
        this->sb = NULL;
    }
    return ret && this->sb == NULL;
}

int BCacheFS_open(BCacheFS *this, const char *path)
{
    int ret = 0;
    this->fp = fopen(path, "rb");
    this->sb = benz_bch_realloc_sb(NULL, 0);
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
        ret = this->sb && benz_bch_fread_sb(this->sb, 0, this->fp);
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
    return this->fp == NULL;
}

int BCacheFS_iter(const BCacheFS *this, BCacheFS_iterator *iter, enum btree_id type)
{
    iter->type = type;
    iter->jset_entry = BCacheFS_iter_next_jset_entry(this, iter);
    iter->btree_node = benz_bch_malloc_btree_node(this->sb);
    return iter->jset_entry && iter->btree_node;
}

int BCacheFS_iter_fini(const BCacheFS *this, BCacheFS_iterator *iter)
{
    printf("BCacheFS_iter_fini\n");
    (void)this;
    iter->type = BTREE_ID_NR;
    iter->jset_entry = NULL;
    iter->btree_ptr = NULL;
    if (iter->btree_node)
    {
        free(iter->btree_node);
        iter->btree_node = NULL;
    }
    iter->bset = NULL;
    return iter->jset_entry == NULL && iter->btree_ptr == NULL &&
            iter->btree_node == NULL && iter->bset == NULL;
}

const struct bch_val *BCacheFS_iter_next(const BCacheFS *this, BCacheFS_iterator *iter)
{
    (void)this;
    const struct bkey *bkey = NULL;
    const struct bkey_dirent *bkey_dirent = NULL;
    if (iter->btree_ptr == NULL && iter->jset_entry)
    {
        iter->btree_ptr = BCacheFS_iter_next_btree_ptr(this, iter);
        if (iter->btree_ptr && !benz_bch_fread_btree_node(iter->btree_node,
                                                          this->sb,
                                                          iter->btree_ptr->start,
                                                          this->fp))
        {
            iter->btree_ptr = NULL;
        }
    }
    if (iter->bset == NULL && iter->jset_entry && iter->btree_ptr)
    {
        iter->bset = BCacheFS_iter_next_bset(this, iter);
    }
    if (iter->jset_entry && iter->btree_ptr && iter->bset) {}
    else
    {
        return NULL;
    }
    switch ((int)iter->type)
    {
        case BTREE_ID_extents:
        iter->bkey = benz_bch_next_bkey(iter->bset, iter->bkey, KEY_TYPE_MAX);
        if (iter->bkey)
        {
            bkey = iter->bkey;
            return benz_bch_next_bch_val(iter->bkey, NULL, 0);
        }
        break;
        case BTREE_ID_dirents:
        iter->bkey = benz_bch_next_bkey_dirent(iter->bset, iter->bkey, 0);
        if (iter->bkey)
        {
            bkey_dirent = iter->bkey;
            return (const void*)benz_bch_dirent(bkey_dirent);
        }
        break;
    }
    if (iter->bkey == NULL)
    {
        iter->bset = BCacheFS_iter_next_bset(this, iter);
    }
    if (iter->bset == NULL)
    {
        iter->btree_ptr = BCacheFS_iter_next_btree_ptr(this, iter);
        if (iter->btree_ptr && !benz_bch_fread_btree_node(iter->btree_node,
                                                          this->sb,
                                                          iter->btree_ptr->start,
                                                          this->fp))
        {
            iter->btree_ptr = NULL;
        }
    }
    if (iter->btree_ptr == NULL)
    {
        iter->jset_entry = BCacheFS_iter_next_jset_entry(this, iter);
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
    const struct bch_val *bch_val = benz_bch_next_bch_val(&jset_entry->start->k,
                                                          (const void*)btree_ptr,
                                                          sizeof(struct bch_btree_ptr_v2));
    btree_ptr = (const void*)bch_val;
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
    const struct bkey *bkey = iter->bkey;
    BCacheFS_extent extent = {.inode = bkey->p.inode};
    benz_bch_file_offset_size(bkey, &extent.file_offset, &extent.offset, &extent.size);
    return extent;
}

BCacheFS_dirent BCacheFS_iter_make_dirent(const BCacheFS *this, BCacheFS_iterator *iter)
{
    (void)this;
    const struct bkey_dirent *bkey_dirent = iter->bkey;
    const struct bch_dirent* bch_dirent = benz_bch_dirent(bkey_dirent);
    return (BCacheFS_dirent){.parent_inode = bkey_dirent->p.inode,
                                  .inode = bch_dirent->d_inum,
                                  .type = bch_dirent->d_type,
                                  .name = bch_dirent->d_name};
}
