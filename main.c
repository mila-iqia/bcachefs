#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcachefs/bcachefs.h"

#define MINI "testdata/mini_bcachefs.img"


int main()
{
    Bcachefs bchfs = {0};
    if (Bcachefs_open(&bchfs, MINI)) {}
    else
    {
        return 1;
    }
    struct bch_sb *sb = bchfs.sb;
    const struct bch_val *bch_val = NULL;
    const struct bch_btree_ptr_v2 *bch_btree_ptr = NULL;
    printf("sb_size: %llu\n", benz_bch_get_sb_size(sb));
    printf("btree_node_size: %llu\n", benz_bch_get_btree_node_size(sb));
    benz_print_uuid(&sb->magic);
    printf("\n");
    uint64_t bset_magic = __bset_magic(sb);
    uint64_t jset_magic = __jset_magic(sb);
    printf("bset_magic:");
    benz_print_hex(((const uint8_t*)&bset_magic) + 0, 4);
    printf("-");
    benz_print_hex(((const uint8_t*)&bset_magic) + 4, 4);
    printf("\n");
    printf("bset_magic:%llu\n", bset_magic);
    printf("jset_magic:");
    benz_print_hex(((const uint8_t*)&jset_magic) + 0, 4);
    printf("-");
    benz_print_hex(((const uint8_t*)&jset_magic) + 4, 4);
    printf("\n");
    printf("jset_magic:%llu\n", jset_magic);

    Bcachefs_iterator bchfs_iter = {0};
    Bcachefs_iter(&bchfs, &bchfs_iter, BTREE_ID_extents);
    bch_val = Bcachefs_iter_next(&bchfs, &bchfs_iter);
    bch_btree_ptr = NULL;
    for (; bch_val; bch_val = Bcachefs_iter_next(&bchfs, &bchfs_iter))
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
        Bcachefs_extent extent = Bcachefs_iter_make_extent(&bchfs, &bchfs_iter);
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
            printf("d:[%s]\n", (const uint8_t*)bch_val);

            printf("file: n:%s, t:%ld\n", fname, ftell(fp));
            fseek(fp, (long)extent.file_offset, SEEK_SET);
            printf("file: n:%s, t:%ld\n", fname, ftell(fp));
            fwrite(bch_val, extent.size, 1, fp);
            break;
        }
        fclose(fp);
    }
    Bcachefs_iter_fini(&bchfs, &bchfs_iter);
    Bcachefs_iter(&bchfs, &bchfs_iter, BTREE_ID_dirents);
    bch_val = Bcachefs_iter_next(&bchfs, &bchfs_iter);
    for (; bch_val; bch_val = Bcachefs_iter_next(&bchfs, &bchfs_iter))
    {
        Bcachefs_dirent dirent = Bcachefs_iter_make_dirent(&bchfs, &bchfs_iter);
        char fname[30] = {0};
        memcpy(fname, dirent.name, dirent.name_len);
        printf("dirent: p:%llu, i:%llu, t:%u, %s\n",
               dirent.parent_inode,
               dirent.inode,
               dirent.type,
               fname);
    }
    Bcachefs_iter_fini(&bchfs, &bchfs_iter);
    Bcachefs_fini(&bchfs);
    return 0;
}
