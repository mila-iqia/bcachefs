import os
import io
import math
import zipfile
from hashlib import sha256

import pytest
from PIL import Image

import bcachefs as bch
from testing import filepath


def pil_loader(file_object):
    img = Image.open(file_object, "r")
    img = img.convert("RGB")
    return img


MINI = "testdata/mini_bcachefs.img"
FILE = "n02033041/n02033041_3834.JPEG"


with zipfile.ZipFile(filepath("testdata/mini_content.zip"), "r") as zipf:
    sha = sha256()
    original_data = zipf.read(os.path.join("mini_content", FILE))
    sha.update(original_data)
    original_hash = sha.digest()


def test_file_properties():
    image = filepath(MINI)

    with bch.mount(image) as fs:
        with fs.open(FILE) as saved:
            assert saved.readable == True
            assert saved.isatty == False
            assert saved.seekable == True
            assert saved.writable == False
            assert saved.fileno() == fs._find_dirent(FILE).inode
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

    with bch.mount(image) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()
            sha.update(saved.readall())
            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash


@pytest.mark.parametrize("size", [1, 2, 4, 8, 16, 32, 1024, 2048])
@pytest.mark.parametrize(
    "read_func",
    [
        bch.bcachefs._BcachefsFileBinary.read,
        bch.bcachefs._BcachefsFileBinary.read1,
    ],
)
def test_file_read(size, read_func):
    image = filepath(MINI)
    assert os.path.exists(image)

    all_data = []
    with bch.mount(image) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()

            k = 0
            data = read_func(saved, size)
            while data:
                all_data.append(data)
                k += 1
                sha.update(data)
                data = read_func(saved, size)

            bcachefs_hash = sha.digest()
            assert (
                sum(len(d) for d in all_data) == saved.tell()
            ), "File tell() should match the size of data"

    all_data = b"".join(all_data)

    assert len(all_data) == len(original_data), "File size should match"
    assert k == math.ceil(len(original_data) / size)
    assert original_hash == bcachefs_hash


@pytest.mark.parametrize("size", [1, 2, 4, 8, 16, 32, 1024, 2048])
def test_file_readinto1(size):
    image = filepath(MINI)
    assert os.path.exists(image)

    with bch.mount(image) as fs:
        with fs.open(FILE) as saved:
            sha = sha256()

            buffer = bytearray(size)
            size = saved.readinto1(buffer)
            while size:
                sha.update(buffer[:size])
                size = saved.readinto1(buffer)

            bcachefs_hash = sha.digest()

    assert original_hash == bcachefs_hash


@pytest.mark.parametrize("offset", [1, 2, 4, 8, 16, 32, 1024, 2048])
def test_file_seek(offset):
    image = filepath(MINI)
    assert os.path.exists(image)

    with bch.mount(image) as fs:
        with fs.open(FILE) as saved:
            saved.seek(offset)
            data = saved.read(offset)
            assert data == original_data[offset : offset * 2]


def test_read_image():
    # no seek works with PIL
    image = filepath(MINI)
    assert os.path.exists(image)

    def no_seek(*args):
        raise io.UnsupportedOperation

    with bch.mount(image) as fs:
        with fs.open(FILE) as image_file:
            image_file.seek = no_seek
            image = pil_loader(image_file)


def test_read_image_with_seek():
    image = filepath(MINI)
    assert os.path.exists(image)

    with bch.mount(image) as fs:
        with fs.open(FILE) as image_file:
            image = pil_loader(image_file)
