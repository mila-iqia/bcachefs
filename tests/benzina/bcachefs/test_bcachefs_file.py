import os
import io
import math
from hashlib import sha256

import pytest
from PIL import Image, ImageFile

import bcachefs.bcachefs as bchfs
from bcachefs import Bcachefs
from bcachefs.testing import filepath


def pil_loader(file_object):
    img = Image.open(file_object, 'r')
    img = img.convert('RGB')
    return img
 

MINI = "testdata/mini_bcachefs.img"
FILE = "n02033041/n02033041_3834.JPEG"


with open(filepath(os.path.join("testdata/mini_content", FILE)), "rb") as original:
    sha = sha256()
    original_data = original.read()
    sha.update(original_data)
    original_hash = sha.digest()


def test_file_properties():
    image = filepath(MINI)

    with Bcachefs(image) as fs:
        with fs.open(FILE) as saved:
            assert saved.readable == True
            assert saved.isatty == False
            assert saved.seekable == True
            assert saved.writable == False
            assert saved.fileno() == fs.find_dirent(FILE).inode
            assert saved.closed == False

            with pytest.raises(io.UnsupportedOperation):
                saved.write(b"whatever")

            with pytest.raises(io.UnsupportedOperation):
                saved.writelines([])

            with pytest.raises(io.UnsupportedOperation):
                saved.detach()


def test_file_readall():
    image = filepath(MINI)
    assert os.path.exists(image)

    with Bcachefs(image) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()
            sha.update(saved.readall())
            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash


@pytest.mark.parametrize("size", [1, 2, 4, 8, 16, 32, 1024, 2048])
def test_file_read1(size):
    image = filepath(MINI)
    assert os.path.exists(image)

    all_data = []
    with Bcachefs(image) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()

            k = 0
            while data := saved.read1(size):
                all_data.append(data)
                k += 1
                sha.update(data)

            bcachefs_hash = sha.digest()

    all_data = b"".join(all_data)

    assert len(all_data) == len(original_data), "File size should match"
    assert k == math.ceil(len(original_data) / size)
    assert original_hash == bcachefs_hash


@pytest.mark.parametrize("size", [1, 2, 4, 8, 16, 32, 1024, 2048])
def test_file_readinto1(size):
    image = filepath(MINI)
    assert os.path.exists(image)

    with Bcachefs(image) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()

            buffer = bytearray(size)
            while size := saved.readinto1(buffer):
                sha.update(buffer[:size])

            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash


@pytest.mark.skip(reason="does not work for PIL")
@pytest.mark.parametrize("offset", [1, 2, 4, 8, 16, 32, 1024, 2048])
def test_file_seek(offset):
    image = filepath(MINI)
    assert os.path.exists(image)

    with Bcachefs(image) as fs:
        with fs.open(FILE) as saved:

            saved.seek(offset)
            data = saved.read(1)
            assert data[0] == original_data[offset]


def test_read_image():
    image = filepath(MINI)
    assert os.path.exists(image)

    with Bcachefs(image) as fs:
        with fs.open(FILE) as image_file:
            image = pil_loader(image_file)
