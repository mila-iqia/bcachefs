Read images
~~~~~~~~~~~

.. code-block:: python

   from bcachefs import Bcachefs

   with Bcachefs('disk.img', 'r') as image:
       file_names = image.namelist()

       for filename in file_names:
           with image.open(filename, 'rb') as file:
               for line in file.lines():
                   print(line)


Multiprocessing
---------------

.. code-block:: python

   import multiprocessing as mp

   def count_size(fs, name):
       with fs:
           try:
               with fs.open(name, 'rb') as f: 
                   return len(f.read()) 
           except FileNotFoundError:
               return 0

   with Bcachefs(image) as fs:
       files = fs.namelist()

       with mp.Pool(4) as p:
           sizes = p.starmap(count_size, [(fs, n) for n in files])

   size = sum(sizes)
