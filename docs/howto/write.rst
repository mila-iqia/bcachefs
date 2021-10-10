Write
~~~~~

BCacheFS focus exclusivelu on reading  BCacheFS archive.
The best way to create a new archive is to use the standard bcachefs tools.

.. code-block:: bash

    # NB: bcachefs-pytools requires a set of system libraries to be present
    # more details at https://github.com/Delaunay/bcachefs-pytools
    pip install git+https://github.com/Delaunay/bcachefs-pytools

    # Get the size of the dataset we want to make an image of
    SIZE=$(du -shc . | tail -n 1 | cut -f 1)

    # Create a file with the size of your dataset
    truncate -s $SIZE dataset_image

    # Format the file using bcachefs file format
    bcachefs format\
        --block_size=4k\
        --metadata_checksum=none\
        --data_checksum=none\
        --compression=none\
        --str_hash=siphash\
        --label=LabelDEADBEEF\
        dataset_image

    # Create a mount point we can write to
    mkdir dataset_mount

    # Mount our image for writing
    bcachefs fusemount -s dataset_image dataset_mount

    # copy our dataset to the image
    cp -r dataset dataset_mount

    # Dismount the image
    fusermount3 -u dataset_mount

    # [Optional] Generate a hash
    sha256sum dataset_image | cut -f 1
