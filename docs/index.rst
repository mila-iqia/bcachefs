BCachefs
========

BCachefs leverage the bcachefs_ filesystem technology to create readonly
dataset image optimized for reading.

Features:

* Parallel Filesystem friendly (Lustre BeeGFS)
* Multiprocessing friendly
* up to 2x faster than HDF5

In progress:

* Encryption
* Compression


.. toctree::
   :caption: How to

   howto/read
   howto/write


.. toctree::

   capi/index
   api/index

.. _bcachefs: https://bcachefs.org/