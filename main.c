#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcachefs/bcachefs.h"

#define MINI "testdata/mini_bcachefs.img"


void iterate_over_dirents(Bcachefs* bchfs);
void iterate_over_extends(Bcachefs* bchfs);
void iterate_over_inodes(Bcachefs* bchfs);

void show_info(Bcachefs* bchfs);


// FLAG(name, default)
#define FLAGS(FLAG)\
    FLAG(dirent, 0)\
    FLAG(extent, 0)\
    FLAG(inode, 0)\
    FLAG(info, 0)

struct Flags {
#define ATTR(name, default)\
    int name;

FLAGS(ATTR)

#undef ATTR
};

struct Flags flags;

void parse_args(int argc, const char* argv[]) {
    #define SET_DEFAULT(name, default)\
        flags.name = default;
    
    FLAGS(SET_DEFAULT)

    #undef SET_DEFAULT

    for(int i = 0; i < argc; i++){
        #define CHECK(name, default)\
            if (strcmp(#name, argv[i]) == 0) { flags.name = 1; }

        FLAGS(CHECK)
        #undef CHECK
    }
}

int main(int argc, const char* argv[])
{
    const char* filename = MINI;

    if (argc > 1) {
        filename = argv[argc - 1];
    }
    parse_args(argc - 1, argv);

    Bcachefs bchfs = {0};
    if (!Bcachefs_open(&bchfs, filename)) {
        printf("File not found\n");
        return 1;
    }

    if (flags.info) {
        show_info(&bchfs);
    }

    if (flags.inode) {
       iterate_over_inodes(&bchfs);
    }

    if (flags.extent) {
       iterate_over_extends(&bchfs);
    }

    if (flags.dirent) {
       iterate_over_dirents(&bchfs);
    }

    return 0;
}


void iterate_over_inodes(Bcachefs* bchfs) {
    Bcachefs_iterator bchfs_iter = {0};
    Bcachefs_iter(&bchfs, &bchfs_iter, BTREE_ID_inodes);
    const struct bch_val * bch_val = Bcachefs_iter_next(bchfs, &bchfs_iter);

    for (; bch_val; bch_val = Bcachefs_iter_next(&bchfs, &bchfs_iter))
    {
        Bcachefs_inode inode = Bcachefs_iter_make_inode(&bchfs, &bchfs_iter);
        printf("inode: i:%llu, s:%llu\n", inode.inode, inode.size);
    }
    Bcachefs_iter_fini(&bchfs, &bchfs_iter);
    Bcachefs_fini(&bchfs);
}

void iterate_over_dirents(Bcachefs* bchfs) {
    Bcachefs_iterator bchfs_iter = {0};
    Bcachefs_iter(&bchfs, &bchfs_iter, BTREE_ID_dirents);
    const struct bch_val * bch_val = Bcachefs_iter_next(bchfs, &bchfs_iter);

    for (; bch_val; bch_val = Bcachefs_iter_next(&bchfs, &bchfs_iter))
    {
        Bcachefs_dirent dirent = Bcachefs_iter_make_dirent(&bchfs, &bchfs_iter);
        printf("dirent: p:%llu, i:%llu, t:%u, %s\n",
               dirent.parent_inode,
               dirent.inode,
               dirent.type,
               dirent.name);
    }
    Bcachefs_iter_fini(&bchfs, &bchfs_iter);
    Bcachefs_fini(&bchfs);
}

void show_info(Bcachefs* bchfs) {
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


void iterate_over_extends(Bcachefs* bchfs) {
    Bcachefs_iterator bchfs_iter = {0};
    Bcachefs_iter(&bchfs, &bchfs_iter, BTREE_ID_extents);
    const struct bch_val * bch_val = Bcachefs_iter_next(bchfs, &bchfs_iter);
    const struct bch_btree_ptr_v2 * bch_btree_ptr = NULL;
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

            fseek(bchfs->fp, (long)extent.offset, SEEK_SET);
            uint8_t *bytes = malloc(extent.size);
            fread(bytes, extent.size, 1, bchfs->fp);
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
}