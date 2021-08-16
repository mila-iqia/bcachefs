import os
import bcachefs.bcachefs as bcachefs
import io
from bcachefs import BCacheFS, Cursor


def test___enter__():
    assert os.path.exists("testdata/mini_bcachefs.img")
    bchfs = BCacheFS("testdata/mini_bcachefs.img")
    assert bchfs.closed
    assert bchfs.size == 0
    with bchfs:
        assert not bchfs.closed
        assert bchfs.size > 0
    assert bchfs.closed
    assert bchfs.size == 0
    del bchfs
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs:
        assert not bchfs.closed
        assert bchfs.size > 0


def test___iter__():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs:
        assert sorted([ent.name for ent in bchfs]) == \
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


def test_find_dirent():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs:
        dirent = bchfs.find_dirent("/")
        assert dirent.parent_inode == 0
        assert dirent.inode == 4096
        assert dirent.type == bcachefs.DIR_TYPE
        assert dirent.name == "/"
        assert dirent == bcachefs.ROOT_DIRENT
        dir_dirent = bchfs.find_dirent("/dir")
        assert dir_dirent.parent_inode == bcachefs.ROOT_DIRENT.inode
        assert dir_dirent.type == bcachefs.DIR_TYPE
        assert dir_dirent.name == "dir"
        subdir_dirent = bchfs.find_dirent("/dir/subdir")
        assert subdir_dirent.parent_inode == dir_dirent.inode
        assert subdir_dirent.type == bcachefs.DIR_TYPE
        assert subdir_dirent.name == "subdir"


def test_ls():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs:
        ls = [e.name for e in bchfs.ls()]
        ls.sort()
        assert ls == ["dir",
                      "file1",
                      "lost+found",
                      "n02033041",
                      "n02445715",
                      "n04467665",
                      "n04584207",
                      "n09332890"]
        ls = [e.name for e in bchfs.ls("/dir")]
        assert ls == ["subdir"]
        ls = [e.name for e in bchfs.ls("/n04467665")]
        assert ls == ["n04467665_63788.JPEG"]


def test_open_file():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs:
        inode = bchfs.find_dirent("file1").inode
        assert bchfs.open_file(inode).read() == b"File content 1\n\0"
        inode = bchfs.find_dirent("dir/subdir/file2").inode
        assert bchfs.open_file(inode).read() == b"File content 2\n\0"
        assert bchfs.open_file("dir/subdir/file2").read() == \
               b"File content 2\n\0"


def test_walk():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs:
        subdir_walk = list(bchfs.walk("dir/subdir"))
        assert len(subdir_walk) == 1

        subdir_dirpath, subdir_dirnames, subdir_filenames = subdir_walk[0]
        subdir_filenames = [f.name for f in subdir_filenames]
        assert len(subdir_dirnames) == 0
        assert subdir_filenames == ["file2"]

        dir_walk = list(bchfs.walk("dir"))
        dir_dirpath, dir_dirnames, dir_filenames = dir_walk[0]
        dir_dirnames = [d.name for d in dir_dirnames]
        assert dir_dirnames == ["subdir"]
        assert len(dir_filenames) == 0

        assert dir_walk[1] == subdir_walk[0]


def test_cursor___iter__():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs, \
            BCacheFS("testdata/mini_bcachefs.img").cd() as cursor:
        assert sorted([ent.name for ent in cursor]) == \
               sorted([ent.name for ent in bchfs])
        cursor.cd("dir")
        assert sorted([ent.name for ent in cursor]) == ["file2",  "subdir"]


def test_cursor_cd():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img").cd() as cursor:
        assert cursor.pwd == "/"
        cursor.cd("dir/subdir")
        assert cursor.pwd == "/dir/subdir"
        cursor.cd("..")
        assert cursor.pwd == "/dir"
        cursor.cd("/dir/subdir")
        assert cursor.pwd == "/dir/subdir"
        cursor.cd("../..")
        assert cursor.pwd == "/"


def test_cursor_find_dirent():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs, \
            BCacheFS("testdata/mini_bcachefs.img").cd() as cursor:
        cursor.cd("dir/subdir")
        assert cursor.find_dirent("file2") == \
               bchfs.find_dirent("dir/subdir/file2")


def test_cursor_ls():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs, \
            BCacheFS("testdata/mini_bcachefs.img").cd() as cursor:
        cursor.cd("dir/subdir")
        assert cursor.ls() == bchfs.ls("dir/subdir")
        cursor.cd()
        assert cursor.ls() == bchfs.ls()


def test_cursor_walk():
    assert os.path.exists("testdata/mini_bcachefs.img")
    with BCacheFS("testdata/mini_bcachefs.img") as bchfs, \
            BCacheFS("testdata/mini_bcachefs.img").cd() as cursor:
        cursor.cd("dir")
        assert list(cursor.walk("subdir")) == list(bchfs.walk("/dir/subdir"))
