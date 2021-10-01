#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcachefs/bcachefs.h"

const char* default_path = "testdata/mini_bcachefs.img";


void print_extent(BCacheFS* bchfs, BCacheFS_iterator* bchfs_iter, const struct bch_val * bch_val) {
    const struct bkey *bkey = bchfs_iter->bkey;
    printf("bkey: u:%u, f:%u, t:%u, s:%u, o:%llu\n", bkey->u64s, bkey->format, bkey->type, bkey->size, bkey->p.offset);
    
    BCacheFS_extent extent = BCacheFS_iter_make_extent(bchfs, bchfs_iter);
    printf("extent: i:%llu fo:%llu, o:%llu, s:%llu\n",
            extent.inode, extent.file_offset, extent.offset, extent.size);
    
    switch (bkey->type)
    {
    case KEY_TYPE_extent:
        break;
    case KEY_TYPE_inline_data:
        printf("inline d:[%s]\n", (const void*)bch_val);
        break;
    }
}

void print_bchs(BCacheFS* bchfs) {
    struct bch_sb *sb = bchfs->sb;
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
}

int main(int argc, const char* argv[])
{
    const char* filename = default_path;

    if (argc > 1) {
        filename = argv[1];
    }

    BCacheFS bchfs = {0};
    if (!BCacheFS_open(&bchfs, filename)) {
        return 1;
    }

    BCacheFS_iterator bchfs_iter = {0};
    
    const struct bch_val * bch_val = NULL;

    {
        BCacheFS_iter(&bchfs, &bchfs_iter, BTREE_ID_extents);
        while (bch_val = BCacheFS_iter_next(&bchfs, &bchfs_iter))
        {
            print_extent(&bchfs, &bchfs_iter, bch_val);
        }
        BCacheFS_iter_fini(&bchfs, &bchfs_iter);
    }

    {
        BCacheFS_iter(&bchfs, &bchfs_iter, BTREE_ID_dirents);
        while (bch_val = BCacheFS_iter_next(&bchfs, &bchfs_iter))
        {
            BCacheFS_dirent dirent = BCacheFS_iter_make_dirent(&bchfs, &bchfs_iter);
            printf("dirent: p:%llu, i:%llu, t:%u, %s\n",
                dirent.parent_inode,
                dirent.inode,
                dirent.type,
                dirent.name);
        }
        BCacheFS_iter_fini(&bchfs, &bchfs_iter);
    }
    BCacheFS_fini(&bchfs);
    return 0;
}
