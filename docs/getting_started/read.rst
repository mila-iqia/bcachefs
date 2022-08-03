Read images
===========

.. code-block:: bash

   python3 -m pip install bcachefs

.. code-block:: python

   import bcachefs as bch

   # Using a filesystem-like API
   with bch.mount("disk.img") as bchfs:
       for root, dirs, files in bchfs.walk():
           print(f"Directories in {root}: {[str(d) for d in dirs]}")
           for ent in files:
               with image.open(ent, "rb") as f:
                   for line in f.lines():
                       print(line)

   # Using a cursor with entries cache
   with bch.mount("disk.img").cd() as cursor:
       for root, dirs, files in cursor.walk():
           print(f"Directories in {root}: {[str(d) for d in dirs]}")
           for ent in files:
               with image.open(ent, "rb") as f:
                   for line in f.lines():
                       print(line)

   # Using a ZipFile-like API
   with Bcachefs("disk.img", "r") as image:
       file_names = image.namelist()

       for filename in file_names:
           with image.open(filename, "rb") as f:
               for line in f.lines():
                   print(line)


Multiprocessing
---------------

.. code-block:: python

   import multiprocessing as mp

   def _count_size(bchfs, entry):
       with bchfs.open(entry, "rb") as f: 
           return len(f.read()) 

   # Using a cursor with entries cache
   with bch.mount("disk.img").cd() as cursor:
       for _, _, files in cursor.walk():
           with mp.Pool(4) as p:
               sizes = p.starmap(_count_size, [(cursor, ent) for ent in files])

   size = sum(sizes)
