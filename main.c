#include <stdio.h>
#include <string.h>

#include "src/libbenzina/bcachefs/bcachefs.h"

int main()
{
    BCacheFS bchfs = {0};
    if (BCacheFS_open(&bchfs, "testdata/mini_bcachefs.img")) {}
    else
    {
        return 1;
    }
    FILE *fp = bchfs.fp;
    struct bch_sb *sb = bchfs.sb;
    const struct bch_sb_field *sb_field = benz_bch_next_sb_field(sb, NULL, BCH_SB_FIELD_clean);
    const struct bch_sb_field_clean* sb_field_clean = (const void*)sb_field;
    const struct jset_entry *jset_entry = benz_bch_next_jset_entry((const void*)sb_field_clean,
                                                                   sizeof(struct bch_sb_field_clean),
                                                                   NULL,
                                                                   BCH_JSET_ENTRY_btree_root);
    const struct bkey_i *bkey_i = NULL;
    const struct bch_val *bch_val = NULL;
    const struct bch_btree_ptr_v2 *bch_btree_ptr = NULL;
    struct btree_node *btree_node = benz_bch_malloc_btree_node(sb);
    struct btree_node *btree_node_extents = benz_bch_malloc_btree_node(sb);
    const struct bset *bset = NULL;
    const struct bset *bset_extents = NULL;
    char buffer[256] = {0};
    printf("sb_size: %llu\n", benz_bch_get_sb_size(sb));
    printf("btree_node_size: %llu\n", benz_bch_get_btree_node_size(sb));
    print_uuid(&sb->magic);
    printf("\n");
    uint64_t bset_magic = __bset_magic(sb);
    uint64_t jset_magic = __jset_magic(sb);
    printf("bset_magic:");
    print_hex(((const uint8_t*)&bset_magic) + 0, 4);
    printf("-");
    print_hex(((const uint8_t*)&bset_magic) + 4, 4);
    printf("\n");
    printf("bset_magic:%llu\n", bset_magic);
    printf("jset_magic:");
    print_hex(((const uint8_t*)&jset_magic) + 0, 4);
    printf("-");
    print_hex(((const uint8_t*)&jset_magic) + 4, 4);
    printf("\n");
    printf("jset_magic:%llu\n", jset_magic);
    for (; jset_entry; jset_entry = benz_bch_next_jset_entry((const void*)sb_field_clean,
                                                             sizeof(struct bch_sb_field_clean),
                                                             jset_entry,
                                                             BCH_JSET_ENTRY_btree_root))
    {
        printf("jset_entry: u:%u, t:%u, bid:%u, l:%u\n", jset_entry->u64s, jset_entry->type, jset_entry->btree_id, jset_entry->level);
        bkey_i = jset_entry->start;
        printf(" bkey_i: u:%u\n", bkey_i->u64s);
        printf("  bkey: u:%u, t:%u, s:%u\n", bkey_i->k.u64s, bkey_i->k.type, bkey_i->k.size);
        bch_val = benz_bch_first_bch_val(&bkey_i->k, BKEY_U64s);
        for(; bch_val; bch_val = benz_bch_next_bch_val(&bkey_i->k, bch_val, sizeof(struct bch_btree_ptr_v2)))
        {
            bch_btree_ptr = (const void*)bch_val;
            if (bch_btree_ptr->start->unused)
            {
                continue;
            }
            printf("   bch_extent_ptr: o:%llu\n", bch_btree_ptr->start->offset);
            benz_bch_fread_btree_node(btree_node, sb, bch_btree_ptr->start, fp);
            bset = benz_bch_next_bset(btree_node, NULL, sb);
            for (; bset; bset = benz_bch_next_bset(btree_node, bset, sb))
            {
                switch (jset_entry->btree_id)
                {
                case BTREE_ID_extents:
                    printf("BTREE_ID_extents\n");
                    memcpy(btree_node_extents, btree_node, benz_bch_get_btree_node_size(sb));
                    bset_extents = (const void*)((const uint8_t*)bset - (const uint8_t*)btree_node +
                                                 (const uint8_t*)btree_node_extents);
                    printf("   bset_extents: u:%u, o:%llu, fo:%llu\n",
                           bset_extents->u64s,
                           (uint64_t)((const uint8_t*)bset_extents - (const uint8_t*)btree_node_extents),
                           benz_bch_get_extent_offset(bch_btree_ptr->start) +
                           (uint64_t)((const uint8_t*)bset_extents - (const uint8_t*)btree_node_extents));
                    break;
                case BTREE_ID_dirents:
                    printf("BTREE_ID_dirents\n");
                    printf("   bset_dirents: u:%u, o:%llu, fo:%llu\n",
                           bset->u64s,
                           (uint64_t)((const uint8_t*)bset - (const uint8_t*)btree_node),
                           benz_bch_get_extent_offset(bch_btree_ptr->start) +
                           (uint64_t)((const uint8_t*)bset - (const uint8_t*)btree_node));
                    {
                        uint64_t inode = benz_bch_find_inode(bset, "/");
                        const struct bkey_short *bkey_short = benz_bch_next_bkey_dirent(bset, NULL, inode);
                        const struct bkey_short *last_bkey_short = NULL;
                        while (bkey_short)
                        {
                            last_bkey_short = bkey_short;
                            const struct bch_dirent *bch_dirent = benz_bch_dirent(bkey_short);
                            const char *full_path = benz_bch_strcpy_file_full_path(buffer + sizeof(buffer), bset, bkey_short);
                            inode = benz_bch_find_inode(bset, full_path);
                            printf("dirent: p:%llu, i:%llu, t:%u, %s\n",
                                   bkey_short->p.inode,
                                   bch_dirent->d_inum,
                                   bch_dirent->d_type,
                                   bch_dirent->d_name);
                            printf("dirent:         i:%llu, %s\n", inode, full_path);

                            if (bch_dirent->d_type == 8)
                            {
                                uint8_t *ptr = benz_bch_malloc_file(bset_extents, inode);
                                uint64_t size = benz_bch_fread_file(ptr, bset_extents, inode, fp);
                                printf("  file:   s:%llu\n", size);
                                char fn[64] = {0};
                                sprintf(fn, "tmp/%s", full_path);
                                FILE *diskfp = fopen(fn, "wb");
                                fwrite(ptr, size, 1, diskfp);
                                fclose(diskfp);
                                ptr = realloc(ptr, 0);
                            }

                            if (bch_dirent->d_type == 4)
                            {
                                bkey_short = benz_bch_next_bkey_dirent(bset, NULL, bch_dirent->d_inum);
                            }
                            else
                            {
                                bkey_short = benz_bch_next_bkey_dirent(bset, bkey_short, bkey_short->p.inode);
                            }

                            while (bkey_short == NULL && last_bkey_short)
                            {
                                bkey_short = benz_bch_parent_bkey_dirent(bset, last_bkey_short);
                                last_bkey_short = bkey_short;
                                if (bkey_short)
                                {
                                    bkey_short = benz_bch_next_bkey_dirent(bset, bkey_short, bkey_short->p.inode);
                                }
                            }
                        }
                    }
                    break;
                    case BTREE_ID_inodes:
                    break;
                }
            }
        }
    }
    printf("\n");
    printf("\n");
    BCacheFS_iterator bchfs_iter = {0};
    BCacheFS_iter(&bchfs, &bchfs_iter, BTREE_ID_extents);
    bch_val = BCacheFS_iter_next(&bchfs, &bchfs_iter);
    bch_btree_ptr = NULL;
    for (; bch_val; bch_val = BCacheFS_iter_next(&bchfs, &bchfs_iter))
    {
        const struct bkey *bkey = bchfs_iter.bkey;
        printf("bkey: u:%u, f:%u, t:%u, s:%u, o:%llu\n", bkey->u64s, bkey->format, bkey->type, bkey->size, bkey->p.offset);
        char fname[30] = {0};
        sprintf(fname, "tmp/%llu.JPEG", bkey->p.inode);
        FILE *fp = fopen(fname, "rb+");
        if (fp == NULL)
        {
            fp = fopen(fname, "wb+");
        }
        if (bch_val == NULL)
        {
            continue;
        }
        BCacheFS_extent extent = BCacheFS_iter_make_extent(&bchfs, &bchfs_iter);
        printf("extent: i:%llu fo:%llu, o:%llu, s:%llu\n",
               extent.inode, extent.file_offset, extent.offset, extent.size);
        switch (bkey->type)
        {
        case KEY_TYPE_extent:
            printf("extent: i:%llu fo:%llu, o:%llu, s:%llu\n",
                   extent.inode, extent.file_offset, extent.offset, extent.size);

            fseek(bchfs.fp, (long)extent.offset, SEEK_SET);
            uint8_t *bytes = malloc(extent.size);
            fread(bytes, extent.size, 1, bchfs.fp);
            printf("file: n:%s, t:%ld\n", fname, ftell(fp));
            fseek(fp, (long)extent.file_offset, SEEK_SET);
            printf("file: n:%s, t:%ld\n", fname, ftell(fp));
            fwrite(bytes, extent.size, 1, fp);
            break;
        case KEY_TYPE_inline_data:
            printf("extent: i:%llu fo:%llu, o:%llu, s:%llu\n",
                   extent.inode, extent.file_offset, extent.offset, extent.size);
            printf("d:[%s]\n", (const void*)bch_val);

            printf("file: n:%s, t:%ld\n", fname, ftell(fp));
            fseek(fp, (long)extent.file_offset, SEEK_SET);
            printf("file: n:%s, t:%ld\n", fname, ftell(fp));
            fwrite(bch_val, extent.size, 1, fp);
            break;
        case KEY_TYPE_btree_ptr_v2:
            bch_btree_ptr = (const void*)bch_val;
            printf("bch_extent_ptr: u:%u, o:%llu\n", bch_btree_ptr->start->unused, bch_btree_ptr->start->offset);
            printf("extent: i:%llu\n", bkey->p.inode);
            bch_val = benz_bch_next_bch_val(bkey, NULL, 0);
            for (; bch_val; bch_val = benz_bch_next_bch_val(bkey, bch_val, sizeof(struct bch_btree_ptr_v2)))
            {
                bch_btree_ptr = (const void*)bch_val;
                printf(" bch_extent_ptr: u:%u, o:%llu\n", bch_btree_ptr->start->unused, bch_btree_ptr->start->offset);
                printf(" extent: i:%llu\n", bkey->p.inode);
            }
            break;
        }
        fclose(fp);
    }
    BCacheFS_iter_fini(&bchfs, &bchfs_iter);
    BCacheFS_iter(&bchfs, &bchfs_iter, BTREE_ID_dirents);
    bch_val = BCacheFS_iter_next(&bchfs, &bchfs_iter);
    for (; bch_val; bch_val = BCacheFS_iter_next(&bchfs, &bchfs_iter))
    {
        BCacheFS_dirent dirent = BCacheFS_iter_make_dirent(&bchfs, &bchfs_iter);
        const char *full_path = benz_bch_strcpy_file_full_path(buffer + sizeof(buffer),
                                                               bchfs_iter.bset,
                                                               bchfs_iter.bkey);
        printf("dirent: p:%llu, i:%llu, t:%u, %s\n",
               dirent.parent_inode,
               dirent.inode,
               dirent.type,
               dirent.name);
        printf("dirent: %s\n", full_path);
    }
    BCacheFS_iter_fini(&bchfs, &bchfs_iter);
    BCacheFS_fini(&bchfs);
    return 0;
}
