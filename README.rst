.. |pypi| image:: https://badge.fury.io/py/bcachefs.svg
   :target: https://pypi.python.org/pypi/bcachefs
   :alt: Current PyPi Version

.. |codecov| image:: https://codecov.io/gh/mila-iqia/bcachefs/branch/master/graph/badge.svg
   :target: https://codecov.io/gh/mila-iqia/bcachefs
   :alt: Codecov Report

.. |docs| image:: https://readthedocs.org/projects/docs/badge/?version=latest
   :target: https://bcachefs.readthedocs.io/en/latest
   :alt: Documentation Status

|pypi| |codecov| |docs|

########
bcachefs
########

C implementation with Python 3.7+ bindings for Bcachefs

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
