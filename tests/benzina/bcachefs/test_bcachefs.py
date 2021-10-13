import os

import pytest

import bcachefs as bchfs
from bcachefs import Bcachefs


MINI = "testdata/mini_bcachefs.img"
LINKS = "testdata/dataset_img"

TEST_ARCHIVE = [MINI, LINKS]

@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test___enter__(archive):
    assert os.path.exists(archive)
    fs = Bcachefs(archive)
    assert fs.closed
    assert fs.size == 0
    with fs:
        assert not fs.closed
        assert fs.size > 0
    assert fs.closed
    assert fs.size == 0
    del fs
    with Bcachefs(archive) as fs:
        assert not fs.closed
        assert fs.size > 0


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test___iter__(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs:
        assert sorted([ent.name for ent in fs]) == \
               ["dir",
                "file1",
                "file2",
                "lost+found",
                "n02033041",
                "n02033041_3834.JPEG",
                "n02445715",
                "n02445715_16523.JPEG",
                "n04467665",
                "n04467665_63788.JPEG",
                "n04584207",
                "n04584207_7936.JPEG",
                "n09332890",
                "n09332890_29876.JPEG",
                "subdir"]


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_find_dirent(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs:
        dirent = fs.find_dirent("/")
        assert dirent.parent_inode == 0
        assert dirent.inode == 4096
        assert dirent.type == bchfs.DIR_TYPE
        assert dirent.name == "/"
        assert dirent == bchfs.ROOT_DIRENT
        dir_dirent = fs.find_dirent("/dir")
        assert dir_dirent.parent_inode == bchfs.ROOT_DIRENT.inode
        assert dir_dirent.type == bchfs.DIR_TYPE
        assert dir_dirent.name == "dir"
        subdir_dirent = fs.find_dirent("/dir/subdir")
        assert subdir_dirent.parent_inode == dir_dirent.inode
        assert subdir_dirent.type == bchfs.DIR_TYPE
        assert subdir_dirent.name == "subdir"


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_ls(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs:
        ls = [e.name for e in fs.ls()]
        ls.sort()
        assert ls == ["dir",
                      "file1",
                      "lost+found",
                      "n02033041",
                      "n02445715",
                      "n04467665",
                      "n04584207",
                      "n09332890"]
        ls = [e.name for e in fs.ls("/dir")]
        assert ls == ["subdir"]
        ls = [e.name for e in fs.ls("/n04467665")]
        assert ls == ["n04467665_63788.JPEG"]


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_read_file(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs:
        inode = fs.find_dirent("file1").inode
        assert fs.read_file(inode) == b"File content 1\n\0"
        inode = fs.find_dirent("dir/subdir/file2").inode
        assert fs.read_file(inode) == b"File content 2\n\0"
        assert fs.read_file("dir/subdir/file2") == \
               b"File content 2\n\0"


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_walk(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs:
        subdir_walk = list(fs.walk("dir/subdir"))
        assert len(subdir_walk) == 1

        subdir_dirpath, subdir_dirnames, subdir_filenames = subdir_walk[0]
        subdir_filenames = [f.name for f in subdir_filenames]
        assert len(subdir_dirnames) == 0
        assert subdir_filenames == ["file2"]

        dir_walk = list(fs.walk("dir"))
        dir_dirpath, dir_dirnames, dir_filenames = dir_walk[0]
        dir_dirnames = [d.name for d in dir_dirnames]
        assert dir_dirnames == ["subdir"]
        assert len(dir_filenames) == 0

        assert dir_walk[1] == subdir_walk[0]


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_cursor___iter__(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs, \
            Bcachefs(archive).cd() as cursor:
        assert sorted([ent.name for ent in cursor]) == \
               sorted([ent.name for ent in fs])
        cursor.cd("dir")
        assert sorted([ent.name for ent in cursor]) == ["file2",  "subdir"]


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_cursor_cd(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive).cd() as cursor:
        assert cursor.pwd == "/"
        cursor.cd("dir/subdir")
        assert cursor.pwd == "/dir/subdir"
        cursor.cd("..")
        assert cursor.pwd == "/dir"
        cursor.cd("/dir/subdir")
        assert cursor.pwd == "/dir/subdir"
        cursor.cd("../..")
        assert cursor.pwd == "/"


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_cursor_find_dirent(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs, \
            Bcachefs(archive).cd() as cursor:
        cursor.cd("dir/subdir")
        assert cursor.find_dirent("file2") == \
               fs.find_dirent("dir/subdir/file2")


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_cursor_ls(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs, \
            Bcachefs(archive).cd() as cursor:
        cursor.cd("dir/subdir")
        assert cursor.ls() == fs.ls("dir/subdir")
        cursor.cd()
        assert cursor.ls() == fs.ls()


@pytest.mark.parametrize("archive", TEST_ARCHIVE)
def test_cursor_walk(archive):
    assert os.path.exists(archive)
    with Bcachefs(archive) as fs, \
            Bcachefs(archive).cd() as cursor:
        cursor.cd("dir")
        assert list(cursor.walk("subdir")) == list(fs.walk("/dir/subdir"))
