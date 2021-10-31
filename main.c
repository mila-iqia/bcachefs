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

    // Extent
    // ----------------------------------------------------------------------------
    {
        Bcachefs_iterator *bchfs_iter = Bcachefs_iter(&bchfs, BTREE_ID_extents);
        bch_val = Bcachefs_iter_next(&bchfs, bchfs_iter);
        bch_btree_ptr = NULL;
        for (int i = 0; bch_val; ++i, bch_val = Bcachefs_iter_next(&bchfs, bchfs_iter))
        {
            Bcachefs_extent extent = Bcachefs_iter_make_extent(&bchfs, bchfs_iter);
            printf("extent %3d: i:%llu fo:%10llu, o:%10llu, s:%10llu\n", 
                i,
                extent.inode, 
                extent.file_offset, 
                extent.offset, 
                extent.size);
        }
        Bcachefs_iter_fini(&bchfs, bchfs_iter);
        free(bchfs_iter);
    }

    // Inode
    // -----------------------------------------------------------------------------
    {
        Bcachefs_iterator *bchfs_iter = Bcachefs_iter(&bchfs, BTREE_ID_inodes);
        bch_val = Bcachefs_iter_next(&bchfs, bchfs_iter);
        uint64_t max_inode = 0;
        for (int i = 0; bch_val; ++i, bch_val = Bcachefs_iter_next(&bchfs, bchfs_iter))
        {
            Bcachefs_inode inode = Bcachefs_iter_make_inode(&bchfs, bchfs_iter);
            printf("inode %3d: i:%llu, s:%10llu, h:%20llu\n", i, inode.inode, inode.size, inode.hash_seed);
            if (max_inode < inode.inode)
            {
                max_inode = inode.inode;
            }
        }
        Bcachefs_iter_fini(&bchfs, bchfs_iter);
        free(bchfs_iter);

        for (int i = 0, ino = BCACHEFS_ROOT_INO; ino <= max_inode; ++i, ++ino)
        {
            Bcachefs_inode inode = Bcachefs_find_inode(&bchfs, ino);
            printf("inode %3d: i:%llu, s:%10llu, h:%20llu\n", i, inode.inode, inode.size, inode.hash_seed);
        }
    }

    // Dirent
    // -----------------------------------------------------------------------------
    {
        Bcachefs_iterator *bchfs_iter = Bcachefs_iter(&bchfs, BTREE_ID_dirents);
        bch_val = Bcachefs_iter_next(&bchfs, bchfs_iter);
        for (int i = 0; bch_val; ++i, bch_val = Bcachefs_iter_next(&bchfs, bchfs_iter))
        {
            Bcachefs_dirent dirent = Bcachefs_iter_make_dirent(&bchfs, bchfs_iter);
            char fname[30] = {0};
            memcpy(fname, dirent.name, dirent.name_len);

            printf("dirent %3d: p:%10llu, i:%10llu, t:%10u, %s\n",
                i,
                dirent.parent_inode,
                dirent.inode,
                dirent.type,
                fname);
        }
        Bcachefs_iter_fini(&bchfs, bchfs_iter);
        free(bchfs_iter);
    }

    Bcachefs_close(&bchfs);
    return 0;
}
