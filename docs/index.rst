########
Bcachefs
########

Bcachefs leverage the `bcachefs <bcachefs_url>`_ filesystem technology to
create readonly disk image optimized for reading.

.. _bcachefs_url: https://bcachefs.org/


********
Features
********

* Parallel Filesystem friendly (Lustre, BeeGFS)
* Multiprocessing friendly
* up to 2x faster than HDF5


*******
Planned
*******

* Encryption
* Compression
* Optimize IO using io_uring
* Readonly mount using FUSE for data exploration


*************
Documentation
*************

.. toctree::

   getting_started/index
   api/index
   contributing/index
