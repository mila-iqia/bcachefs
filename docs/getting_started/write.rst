Creating a new disk image
~~~~~~~~~~~~~~~~~~~~

Bcachefs focus exclusively on reading Bcachefs archive.
The best way to create a new archive is to use the standard bcachefs tools.

.. code-block:: bash

   sudo apt-get update
   sudo apt install gcc meson ninja-build
   sudo apt install make pkg-config libaio-dev libblkid-dev\
        libkeyutils-dev liblz4-dev libscrypt-dev libsodium-dev\
        liburcu-dev libzstd-dev libudev-dev uuid-dev zlib1g-dev\
        libfuse3-dev

   # NB: bcachefs-pytools requires a set of system libraries to be present
   # more details at https://github.com/Delaunay/bcachefs-pytools
   pip install git+https://github.com/Delaunay/bcachefs-pytools

   # Get the size of the disk image we want to create
   SIZE=$(du -shc dataset | tail -n 1 | cut -f 1)

   # Create a file with the size of your dataset
   truncate -s $SIZE disk.img

   # Format the file using bcachefs file format
   bcachefs format\
        --block_size=4k\
        --metadata_checksum=none\
        --data_checksum=none\
        --compression=none\
        --str_hash=siphash\
        --label=LabelDEADBEEF\
        disk.img

   # Create a mount point we can write to
   mkdir disk_mount

   # Mount our image for writing
   bcachefs fusemount -s disk.img disk_mount

   # copy our files to the disk image
   cp -r dataset dataset_mount

   # check the checksum to make sure sure the copy worked
   original=$(find dataset -type f -exec md5sum {} \; | cut -d ' ' -f 1 | sort | md5sum)
   backup=$(find tmp/ -type f -exec md5sum {} \; | cut -d ' ' -f 1 | sort | md5sum)

   echo "$original == $backup"

   # Dismount the image
   fusermount3 -u dataset_mount
