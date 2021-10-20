import os
from hashlib import sha256

import pytest

import bcachefs.bcachefs as bchfs
from bcachefs import Bcachefs

MINI = "testdata/mini_bcachefs.img"
FILE = "n02033041/n02033041_3834.JPEG"

this = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.abspath(os.path.join(this, "..", "..", ".."))


def file(path):
    return os.path.join(project_root, path)


with open(file(os.path.join("testdata/mini_content", FILE)), 'rb') as original:
    sha = sha256()
    sha.update(original.read())
    original_hash = sha.digest()


def test_file_properties():
    with Bcachefs(file("testdata/mini_bcachefs.img")) as fs:
        with fs.open(FILE) as saved:
            assert saved.readable == True
            assert saved.isatty == False
            assert saved.seekable == False
            assert saved.writable == False
            assert saved.fileno() == fs.find_dirent(FILE).inode
            assert saved.closed == False

            with pytest.raises(io.UnsupportedOperation):
                saved.write(b'whatever')

            with pytest.raises(io.UnsupportedOperation):
                saved.writelines([])
            
            with pytest.raises(io.UnsupportedOperation):
                saved.detach()
            

def test_file_readall():
    assert os.path.exists(file("testdata/mini_bcachefs.img"))

    with Bcachefs(file("testdata/mini_bcachefs.img")) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()
            sha.update(original.readall())
            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash


def test_file_read1():
    with Bcachefs(file("testdata/mini_bcachefs.img")) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()

            while data := saved.read1(1):
                sha.update(data)
            
            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash


def test_file_readinto1():
    with Bcachefs(file("testdata/mini_bcachefs.img")) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()

            buffer = bytearray(2)
            while size := saved.readinto1(buffer):
                sha.update(buffer[:size])
            
            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash
