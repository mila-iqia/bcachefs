Read
~~~~


.. code-block:: python

   from bcachefs import BCacheFS

   with BCacheFS('dataset_img', 'r') as archive:
       files = archive.namelist()
       print(files)

       with archive.open(files[0], 'r') as file:
           for line in file.lines():
               print(line)

