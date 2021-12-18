Creating a new disk image
=========================

Bcachefs focus exclusively on reading Bcachefs archive. The easiest way to create
a new archive is to use the standard bcachefs-tools.

.. code-block:: bash

   > NAME=bcachefs.img CONTENT_SRC=content/ scripts/make_disk_image.sh
   # External UUID:                  ec05f4c8-d3c8-440e-b4d4-c1adca266f92
   # Internal UUID:                  59a6e1f3-a879-43a5-a3c4-fcb726a720a1
   # Device index:                   0
   # Label:                          LabelDEADBEEF
   # Version:                        14
   # Oldest version on disk:         14
   # Created:                        Fri Dec 17 20:34:30 2021
   # Squence number:                 0
   # Block_size:                     4.0K
   # Btree node size:                128.0K
   # Error action:                   ro
   # Clean:                          0
   # Features:                       new_siphash,new_extent_overwrite,btree_ptr_v2,extents_above_btree_updates,btree_updates_journalled,new_varint,journal_no_flush,alloc_v2,extents_across_btree_nodes
   # Compat features:
   # Metadata replicas:              1
   # Data replicas:                  1
   # Metadata checksum type:         none (0)
   # Data checksum type:             none (0)
   # Compression type:               none (0)
   # Foreground write target:        none
   # Background write target:        none
   # Promote target:                 none
   # Metadata target:                none
   # String hash type:               siphash (2)
   # 32 bit inodes:                  1
   # GC reserve percentage:          8%
   # Root reserve percentage:        0%
   # Devices:                        1 live, 1 total
   # Sections:                       members
   # Superblock size:                816
   #
   # Members (size 64):
   #   Device 0:
   #     UUID:                       df2cca3d-6a2b-45b0-a601-76a45e928bc5
   #     Size:                       10.0M
   #     Bucket size:                128.0K
   #     First bucket:               0
   #     Buckets:                    80
   #     Last mount:                 (never)
   #     State:                      rw
   #     Group:                      (none)
   #     Data allowed:               journal,btree,user
   #     Has data:                   (none)
   #     Replacement policy:         lru
   #     Discard:                    0
   # initializing new filesystem
   # going read-write
   # mounted with opts: metadata_checksum=none,data_checksum=none
   # INFO:    Converting SIF file to temporary sandbox...
   # INFO:    instance started successfully
   # Opening bcachefs filesystem on:
   #         /bch/disk.img
   # recovering from clean shutdown, journal seq 4
   # going read-write
   # mounted with opts: metadata_checksum=none,data_checksum=none
   # Fuse mount initialized.
   # fuse_init: activating writeback
   #
   # ============================================
   #
   # Run the following commands in another shell:
   #         pushd '[...]/bcachefs/scripts' && UNMOUNT=1 ./cp.sh . && popd || popd
   #
   # ============================================

.. code-block:: bash

   > pushd '[...]/bcachefs/scripts' && UNMOUNT=1 ./cp.sh . && popd || popd
   # [...]/bcachefs/scripts ~
   #
   # ============================================
   #
   # Unmounting ...
   #
   # ============================================
   #
   # INFO:    Stopping bcachefs instance of /tmp/rootfs-786675488/root (PID=30318)
   # ~
