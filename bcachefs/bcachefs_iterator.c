#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bcachefs_iterator.h"

#include "libbenzina/bcachefs.h"
#include "libbenzina/siphash.h"


int Bcachefs_open(Bcachefs *this, const char *path)
{
    *this = BCACHEFS_CLEAN;

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
        this->_iter = Bcachefs_iter(this, BTREE_ID_NR);
    }
    if (ret)
    {
        this->_root_stats = Bcachefs_find_inode(this, BCACHEFS_ROOT_INO);
        this->_root_dirent = (Bcachefs_dirent){.parent_inode=BCACHEFS_ROOT_INO,
                                               .inode=BCACHEFS_ROOT_INO,
                                               .type=4,
                                               .name=(const void*)"",
                                               .name_len=0};
        ret = this->_root_stats.hash_seed != 0;
    }
    if (!ret)
    {
        Bcachefs_close(this);
    }
    return ret && this->fp && this->sb && this->_iter;
}

int Bcachefs_close(Bcachefs *this)
{
    this->_root_stats = (Bcachefs_inode){0};
    this->_root_dirent = (Bcachefs_dirent){0};
    int ret = Bcachefs_iter_fini(this, &this->_extents_iter_begin) &&
        Bcachefs_iter_fini(this, &this->_inodes_iter_begin) &&
        Bcachefs_iter_fini(this, &this->_dirents_iter_begin);
    if (this->_iter && Bcachefs_iter_fini(this, this->_iter))
    {
        free(this->_iter);
        this->_iter = NULL;
    }
    if (this->fp && !fclose(this->fp))
    {
        this->fp = NULL;
        this->size = 0;
    }
    free(this->sb);
    this->sb = NULL;
    return ret && this->fp == NULL && this->sb == NULL && this->_iter == NULL;
}

Bcachefs_iterator* Bcachefs_iter(const Bcachefs *this, enum btree_id type)
{
    Bcachefs_iterator *iter = malloc(sizeof(Bcachefs_iterator));
    *iter = BCACHEFS_ITERATOR_CLEAN;
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
    if (!iter_begin) {} // return clean iterator
    else if (Bcachefs_iter_minimal_copy(this, iter, iter_begin)) {}
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
        .jset_entry = iter->jset_entry,
        .btree_ptr = btree_ptr,
        .btree_node = benz_bch_malloc_btree_node(this->sb)
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

int _Bcachefs_comp_bkey_lesser_than(struct bkey_local_buffer *buffer, struct bkey_local_buffer *reference)
{
    enum bch_bkey_fields field = 0;
    for (; field < BKEY_NR_FIELDS && buffer->buffer[field] == reference->buffer[field]; ++field) {}
    return field < BKEY_NR_FIELDS && buffer->buffer[field] < reference->buffer[field];
}

int _Bcachefs_comp_bkey_lesseq_than(struct bkey_local_buffer *buffer, struct bkey_local_buffer *reference)
{
    enum bch_bkey_fields field = 0;
    for (; field < BKEY_NR_FIELDS && buffer->buffer[field] == reference->buffer[field]; ++field) {}
    return field == BKEY_NR_FIELDS || buffer->buffer[field] < reference->buffer[field];
}

const struct bkey* _Bcachefs_find_bkey(const Bcachefs *this, Bcachefs_iterator *iter, struct bkey_local_buffer *reference, int start_pos)
{
    struct bkey_local_buffer bkey_value = {{0}};
    const struct bkey *bkey;

    int pos = start_pos;
    for (; pos < iter->num_keys; ++pos)
    {
        bkey = iter->keys[pos];
        switch ((int)iter->type)
        {
        case BTREE_ID_inodes:
        case BTREE_ID_dirents:
            bkey_value = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_FIELD_OFFSET + 1);
            break;
        case BTREE_ID_extents:
            bkey_value = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_FIELD_SIZE + 1);
            bkey_value.buffer[BKEY_FIELD_OFFSET] -= bkey_value.buffer[BKEY_FIELD_SIZE];
            memset(&bkey_value.buffer[BKEY_FIELD_OFFSET + 1], 0, (BKEY_FIELD_SIZE - BKEY_FIELD_OFFSET) * sizeof(*bkey_value.buffer));
            break;
        }
        if (!_Bcachefs_comp_bkey_lesser_than(&bkey_value, reference)) break;
    }

    if (pos == iter->num_keys) return NULL;

    uint8_t key_u64s = bkey->format == KEY_FORMAT_LOCAL_BTREE ?
      iter->btree_node->format.key_u64s : BKEY_U64s;
    iter->bch_val = benz_bch_first_bch_val(bkey, key_u64s);

    if (bkey->type == KEY_TYPE_btree_ptr_v2)
    {
        uint8_t key_u64s = bkey->format == KEY_FORMAT_LOCAL_BTREE ?
          iter->btree_node->format.key_u64s : BKEY_U64s;
        iter->bkey = bkey;
        iter->bch_val = benz_bch_first_bch_val(bkey, key_u64s);
        const struct bch_btree_ptr_v2* btree_ptr = (const void*)iter->bch_val;
        bkey_value.buffer[BKEY_FIELD_INODE] = btree_ptr->min_key.inode;
        bkey_value.buffer[BKEY_FIELD_OFFSET] = btree_ptr->min_key.offset;
        if (bkey_value.buffer[BKEY_FIELD_OFFSET])
        {
            // for some reason min_key can contain inode == ref inode and have a offset == ref offset + 1
            bkey_value.buffer[BKEY_FIELD_OFFSET] -= 1;
        }
        memset(&bkey_value.buffer[BKEY_FIELD_OFFSET + 1], 0, (BKEY_FIELD_SIZE - BKEY_FIELD_OFFSET) * sizeof(*bkey_value.buffer));
        if (iter->next_it && Bcachefs_iter_fini(this, iter->next_it))
        {
            free(iter->next_it);
            iter->next_it = NULL;
        }
        if (_Bcachefs_comp_bkey_lesseq_than(&bkey_value, reference) && !iter->next_it &&
            Bcachefs_next_iter(this, iter, btree_ptr))
        {
            const struct bkey* bkey = _Bcachefs_find_bkey(this, iter->next_it, reference, 0);
            if (bkey)
            {
                return bkey;
            }
	    // some bkeys sequence (like extents) could be spread over multiple
	    // btrees. if the precedent btree doesn't contain the desired bkey,
	    // continue to search from the current btree.
            else
            {
                return _Bcachefs_find_bkey(this, iter, reference, ++pos);
            }
        }
    }
    else if (!memcmp(bkey_value.buffer, reference->buffer, sizeof(bkey_value.buffer)))
    {
        uint8_t key_u64s = bkey->format == KEY_FORMAT_LOCAL_BTREE ?
          iter->btree_node->format.key_u64s : BKEY_U64s;
        iter->bkey = bkey;
        iter->bch_val = benz_bch_first_bch_val(bkey, key_u64s);
        return bkey;
    }
    return NULL;
}

Bcachefs_extent Bcachefs_find_extent(Bcachefs *this, uint64_t inode, uint64_t file_offset)
{
    Bcachefs_extent extent = {0};
    struct bkey_local_buffer reference = {{0}};
    reference.buffer[BKEY_FIELD_INODE] = inode;
    reference.buffer[BKEY_FIELD_OFFSET] = file_offset / BCH_SECTOR_SIZE + file_offset % BCH_SECTOR_SIZE;
    if (Bcachefs_iter_fini(this, this->_iter))
    {
        free(this->_iter);
        this->_iter = Bcachefs_iter(this, BTREE_ID_extents);
    }
    else
    {
        return extent;
    }
    Bcachefs_iterator *iter = this->_iter;
    const struct bkey *bkey = _Bcachefs_find_bkey(this, iter, &reference, 0);
    if (bkey)
    {
        switch (bkey->type)
        {
        case KEY_TYPE_deleted:
            extent.inode = inode;
        default:
            extent = Bcachefs_iter_make_extent(this, iter);
        }
    }
    return extent;
}

Bcachefs_inode Bcachefs_find_inode(Bcachefs *this, uint64_t inode)
{
    if (inode == this->_root_stats.inode)
    {
        return this->_root_stats;
    }
    Bcachefs_inode stats = {0};
    struct bkey_local_buffer reference = {{0}};
    reference.buffer[BKEY_FIELD_OFFSET] = inode;
    if (Bcachefs_iter_fini(this, this->_iter))
    {
        free(this->_iter);
        this->_iter = Bcachefs_iter(this, BTREE_ID_inodes);
    }
    else
    {
        return stats;
    }
    Bcachefs_iterator *iter = this->_iter;
    const struct bkey *bkey = _Bcachefs_find_bkey(this, iter, &reference, 0);
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
    return stats;
}

Bcachefs_dirent Bcachefs_find_dirent(Bcachefs *this, uint64_t parent_inode, uint64_t hash_seed, const uint8_t *name, const uint8_t len)
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
    struct bkey_local_buffer reference = {{0}};
    reference.buffer[BKEY_FIELD_INODE] = parent_inode;
    reference.buffer[BKEY_FIELD_OFFSET] = offset;
    if (Bcachefs_iter_fini(this, this->_iter))
    {
        free(this->_iter);
        this->_iter = Bcachefs_iter(this, BTREE_ID_dirents);
    }
    else
    {
        return dirent;
    }
    Bcachefs_iterator *iter = this->_iter;
    const struct bkey *bkey = _Bcachefs_find_bkey(this, iter, &reference, 0);
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
    return dirent;
}

// a < b
int _bkey_packed_less(const struct bkey *a, const struct bkey *b, const struct bkey_format *fmt)
{
    if (a->format != b->format) abort();

    switch (a->format)
    {
    case KEY_FORMAT_LOCAL_BTREE:
        ; // WTF??? This needs to be here or the var decl won't parse WTF???
        const uint8_t *bytes_a = (const void *)a;
        const uint8_t *bytes_b = (const void *)b;
        enum bch_bkey_fields i;

        bytes_a += fmt->key_u64s * BCH_U64S_SIZE;
        bytes_b += fmt->key_u64s * BCH_U64S_SIZE;
        for (i = 0; i < BKEY_NR_FIELDS; ++i)
        {
            bytes_a -= fmt->bits_per_field[i] / 8;
            bytes_b -= fmt->bits_per_field[i] / 8;
            if (benz_uintXX_as_uint64(bytes_a, fmt->bits_per_field[i]) !=
                benz_uintXX_as_uint64(bytes_b, fmt->bits_per_field[i])) break;
        }
        int res = i < BKEY_NR_FIELDS &&
          benz_uintXX_as_uint64(bytes_a, fmt->bits_per_field[i]) <
          benz_uintXX_as_uint64(bytes_b, fmt->bits_per_field[i]);

        return res;
    case KEY_FORMAT_CURRENT:
        if (a->p.inode != b->p.inode)
          return a->p.inode < b->p.inode;
        if (a->p.offset != b->p.offset)
          return a->p.offset < b->p.offset;
        if (a->p.snapshot != b->p.snapshot)
          return a->p.snapshot < b->p.snapshot;
        if (a->size != b->size)
          return a->size < b->size;
        if (a->version.hi != b->version.hi)
          return a->version.hi < b->version.hi;
        return a->version.lo < b->version.lo;
    }
    // If the format is not what we expect
    abort();
}

void _Bcachefs_iter_build_bsets_cache(const Bcachefs *this, Bcachefs_iterator *iter)
{
    // We first get all the bsets from the node
    uint32_t num = 8;  // Initial number of bsets for allocation
    const struct bset **bsets = malloc(sizeof(struct bset *) * num);
    const struct bset **cursor = bsets;
    const struct bset **bsets_end = NULL;
    const struct bset *ptr = NULL;

    const void *btree_node_end = (const uint8_t *)iter->btree_node + iter->btree_ptr->sectors_written * BCH_SECTOR_SIZE;

    while ((ptr = benz_bch_next_bset(iter->btree_node, btree_node_end, ptr, this->sb)))
    {
        *(cursor++) = ptr;
        if (cursor == (bsets + num))
        {
            num = num * 2;
            bsets = realloc(bsets, sizeof(struct bset *) * num);
            cursor = (bsets + (num / 2));
        }
    }

    // Because those are properly typed it will count the number of items, not bytes
    num = cursor - bsets;

    bsets_end = cursor - 1;

    // Now we do a mergesort with all the bsets

    // The initial sizes are guesswork, maybe we can do better
    uint32_t knum = 1024;
    uint32_t bpos;
    // These will be the pointer to the current bkey for all the sets
    iter->keys = calloc(knum, sizeof(struct bkey *)); // an array of 1024 pointers to start
    const struct bkey **kcursors = calloc(num, sizeof (struct bkey *));
    const struct bkey **kcursors_end = kcursors + (num - 1);
    // helper for iteration
    const struct bkey **kptr;
    // This is the pointer to the next slot in iter->keys
    const struct bkey **next = iter->keys;
    // This is the pointer to the current canditate for next slot (lowest amongst all the bsets)
    const struct bkey *best = NULL;
    const struct bkey *last = NULL;

    // Set up all key pointers to the first key in each bset
    for (cursor = bsets_end, kptr = kcursors_end; cursor >= bsets; --cursor, --kptr)
        *kptr = benz_bch_next_bkey(*cursor, NULL, KEY_TYPE_MAX);

    for (;;)
    {
        for (kptr = kcursors_end; kptr >= kcursors; --kptr)
        {
            if (*kptr == NULL) continue;  // we are at the end of that btree
            // skip over invalid keys (that are smaller than the last key we accepted)
            uint32_t pos = kptr - kcursors;
            if (last != NULL)
            {
                while (!_bkey_packed_less(last, *kptr, &iter->btree_node->format))
                {
                    *kptr = benz_bch_next_bkey(bsets[pos], *kptr, KEY_TYPE_MAX);
                    if (*kptr == NULL) break;
                }
            }
            // We've reached the end of the current bset
            if (*kptr == NULL) continue;

            // Look for the smallest key that is left. Since we look
            // from the most recent bset, a duplicate will use the
            // most recent key
            if (best == NULL || _bkey_packed_less(*kptr, best, &iter->btree_node->format))
            {
                best = *kptr;
                bpos = pos;
            }
        }
        // This is the exit condition for the loop normally
        if (best == NULL) break;

        last = best;
        // Update the cursor for the best key
        kcursors[bpos] = benz_bch_next_bkey(bsets[bpos], kcursors[bpos], KEY_TYPE_MAX);
        // Record the key if it is valid
        if (best->type != KEY_TYPE_deleted && best->type != KEY_TYPE_hash_whiteout)
        {
            *next++ = best;
            // Grow the keys array if we reached the end
            if (next == iter->keys + knum)
            {
                uint32_t cur_pos = next - iter->keys;
                knum *= 2;
                iter->keys = realloc(iter->keys, sizeof(struct bkey *) * knum);
                next = iter->keys + cur_pos;
            }
        }
        best = NULL;
    }
    // Mark the end of the array
    *next++ = NULL;
    iter->num_keys = knum = (next - iter->keys) - 1;
    iter->pos = 0;
    free(bsets);

    // Trim the allocation to size to avoid wasting memory
    iter->keys = realloc(iter->keys, sizeof(struct bkey *) * (knum + 1));
}

int Bcachefs_iter_reinit(const Bcachefs *this, Bcachefs_iterator *iter, enum btree_id type)
{
    if (!memcmp(iter, &BCACHEFS_ITERATOR_CLEAN, sizeof(Bcachefs_iterator)))
    {
        // Initialize the iterator
        iter->type = type;
        iter->jset_entry = Bcachefs_iter_next_jset_entry(this, iter);
        iter->btree_ptr = Bcachefs_iter_next_btree_ptr(this, iter);
        iter->btree_node = benz_bch_malloc_btree_node(this->sb);
    }
    else
    {
        // Reinitialize the btree pointers using the existing btree
        Bcachefs_iter_fini(this, iter->next_it);
        iter->next_it = NULL;
        if (iter->keys)
        {
            free(iter->keys);
            iter->keys = NULL;
        }
        *iter = (Bcachefs_iterator){
            .type = iter->type,
            .jset_entry = iter->jset_entry,
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
    if (memcmp(iter, &BCACHEFS_ITERATOR_CLEAN, sizeof(Bcachefs_iterator)))
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

    if (other->num_keys)
    {
        iter->keys = malloc(sizeof(struct bset *) * other->num_keys);
        iter->num_keys = other->num_keys;
        for (uint32_t i = 0; i < iter->num_keys; ++i)
        {
          const uint64_t offset = (const uint8_t *)other->keys[i] - (const uint8_t *)other->btree_node;
          iter->keys[i] = (const void *)((const uint8_t *)iter->btree_node + offset);
        }
    }
    else
    {
        iter->keys = NULL;
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
    free(iter->btree_node);
    iter->btree_node = NULL;
    free(iter->keys);
    iter->keys = NULL;
    *iter = (Bcachefs_iterator){
        .type = BTREE_ID_NR,
        .btree_node = iter->btree_node,
        .next_it = iter->next_it,
        .keys = iter->keys
    };
    return iter->next_it == NULL && iter->btree_node == NULL &&
        iter->keys == NULL;
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
    if (iter->pos < iter->num_keys)
    {
        iter->bkey = iter->keys[iter->pos++];
        bch_val = _Bcachefs_iter_next_bch_val(iter->bkey, &iter->btree_node->format);
    }
    else
    {
        return NULL;
    }
    switch ((int)iter->type)
    {
    case BTREE_ID_extents:
    case BTREE_ID_inodes:
    case BTREE_ID_dirents:
        iter->bch_val = bch_val;
        if (bch_val && iter->bkey->type == KEY_TYPE_btree_ptr_v2 &&
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

Bcachefs_extent Bcachefs_iter_make_extent(const Bcachefs *this, Bcachefs_iterator *iter)
{
    (void)this;

    while (iter->next_it)
    {
        iter = iter->next_it;
    }

    const struct bkey_local_buffer buffer = benz_bch_parse_bkey_buffer(iter->bkey, &iter->btree_node->format, BKEY_NR_FIELDS);
    const struct bkey_local bkey_local = benz_bch_parse_bkey(iter->bkey, &buffer);
    const struct bkey *bkey = (const void*)&bkey_local;
    switch (bkey->type)
    {
    case KEY_TYPE_extent:
    case KEY_TYPE_inline_data:
        break;
    default:
        return (Bcachefs_extent){0};
    }

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
    const struct bkey_local_buffer buffer = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_NR_FIELDS);
    const struct bkey_local bkey_local = benz_bch_parse_bkey(bkey, &buffer);
    const struct bch_inode *bch_inode = (const void*)iter->bch_val;
    switch (bkey->type)
    {
    case KEY_TYPE_inode:
        break;
    default:
        return (Bcachefs_inode){0};
    }

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
    const struct bkey_local_buffer buffer = benz_bch_parse_bkey_buffer(bkey, &iter->btree_node->format, BKEY_NR_FIELDS);
    const struct bkey_local bkey_local = benz_bch_parse_bkey(bkey, &buffer);
    const struct bch_dirent *bch_dirent = (const void*)iter->bch_val;
    switch (bkey->type)
    {
    case KEY_TYPE_dirent:
        break;
    default:
        return (Bcachefs_dirent){0};
    }

    const uint8_t name_len = strlen((const void*)bch_dirent->d_name);
    const uint8_t max_name_len = (const uint8_t*)bkey + bkey->u64s * BCH_U64S_SIZE - bch_dirent->d_name;
    return (Bcachefs_dirent){.parent_inode = bkey_local.p.inode,
                                  .inode = bch_dirent->d_inum,
                                  .type = bch_dirent->d_type,
                                  .name = bch_dirent->d_name,
                                  .name_len = (name_len < max_name_len ? name_len : max_name_len)};
}
