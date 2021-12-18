import os
import multiprocessing as mp

import numpy as np
import pytest

import bcachefs as bch
from testing import filepath


MINI = "testdata/mini_bcachefs.img"
MANY = "testdata/many_bcachefs.img"

_TEST_IMAGES = [filepath(MINI), filepath(MANY)]


@pytest.fixture(scope="module", params=[MINI, MANY])
def filesystem(request) -> bch.Bcachefs:
    image = request.param
    assert os.path.exists(image)
    with bch.mount(image) as bchfs:
        yield bchfs


@pytest.fixture(scope="module")
def cursor(filesystem: bch.Bcachefs) -> bch.Cursor:
    return filesystem.cd()


@pytest.fixture(scope="module")
def dir_cursor(filesystem: bch.Bcachefs) -> bch.Cursor:
    for root, dirs, _ in filesystem.walk():
        if dirs:
            dirs.sort(key=lambda ent: ent.inode)
            # skip lost+found
            return filesystem.cd(os.path.join(root, str(dirs[1])))


@pytest.fixture(params=["filesystem", "cursor"])
def bchfs(filesystem, cursor, request) -> bch.Bcachefs:
    filesystem, cursor
    kwargs = {**locals()}
    return kwargs[request.param]


@pytest.fixture(autouse=True)
def skip_if_not_image(request, bchfs: bch.Bcachefs):
    images_only = request.node.get_closest_marker("images_only")
    if images_only and bchfs.filename not in images_only.args[0]:
        pytest.skip(f"{images_only.args[0]} test only")


@pytest.mark.parametrize("image", _TEST_IMAGES)
def test_mount(image):
    image = filepath(image)
    assert os.path.exists(image)

    bchfs = bch.mount(image)
    assert not bchfs.unmounted
    assert list(bchfs)
    bchfs.umount()
    assert bchfs.unmounted
    assert not list(bchfs)

    with bch.mount(image) as bchfs:
        assert not bchfs.unmounted
        assert list(bchfs)


def test___iter__(bchfs: bch.Bcachefs):
    if bchfs.filename.endswith(MINI):
        assert sorted([str(ent) for ent in bchfs]) == [
            "dir",
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
            "subdir",
        ]
    elif bchfs.filename.endswith(MANY):
        assert len(set(bchfs)) == (
            # hardlinks + dirs  + 0 (original file) + lost+found
            1500 * 25
            + 25
            + 1
            + 1
        )


@pytest.mark.images_only([MINI])
def test_scandir(bchfs: bch.Bcachefs):
    ls = [str(ent) for ent in bchfs.scandir()]
    assert sorted(ls) == [
        "dir",
        "file1",
        "lost+found",
        "n02033041",
        "n02445715",
        "n04467665",
        "n04584207",
        "n09332890",
    ]
    ls = [str(ent) for ent in bchfs.scandir("/dir")]
    assert ls == ["subdir"]
    ls = [str(ent) for ent in bchfs.scandir("/n04467665")]
    assert ls == ["n04467665_63788.JPEG"]


def test_read(bchfs: bch.Bcachefs):
    if bchfs.filename.endswith(MINI):
        assert bchfs.read("file1") == b"File content 1\n"
        assert bchfs.read("dir/subdir/file2") == b"File content 2\n"
    elif bchfs.filename.endswith(MANY):
        f0 = bchfs.read("0")
        assert f0 == b"test content\n"

        for ent in set(bchfs):
            if ent.is_dir:
                continue
            assert bchfs.read(ent) == f0


def test_readinto(bchfs: bch.Bcachefs):
    buffer = np.empty(20, dtype="<u1")
    buffer = memoryview(buffer)
    if bchfs.filename.endswith(MINI):
        _len = bchfs.readinto("file1", buffer)
        assert _len == len(b"File content 1\n")
        assert buffer[:_len] == b"File content 1\n"
        _len = bchfs.readinto("dir/subdir/file2", buffer)
        assert _len == len(b"File content 2\n")
        assert buffer[:_len] == b"File content 2\n"
    elif bchfs.filename.endswith(MANY):
        f0 = bchfs.read("0")
        assert f0 == b"test content\n"

        for ent in set(bchfs):
            if ent.is_dir:
                continue
            _len = bchfs.readinto(ent.inode, buffer)
            assert _len == len(f0)
            assert buffer[:_len] == f0


@pytest.mark.images_only([MINI])
def test_cd(bchfs: bch.Bcachefs):
    with bchfs.cd() as cursor:
        assert cursor.pwd == ""
    with bchfs.cd("dir/subdir") as cursor:
        assert cursor.pwd == "dir/subdir"
        with cursor.cd() as c:
            assert c.pwd == ""
        with cursor.cd("/") as c:
            assert c.pwd == ""


@pytest.mark.images_only([MINI])
def test_walk(bchfs: bch.Bcachefs):
    subdir_walk = list(bchfs.walk("dir/subdir"))
    assert len(subdir_walk) == 1

    _, subdir_dirnames, subdir_filenames = subdir_walk[0]
    subdir_filenames = [str(f) for f in subdir_filenames]
    assert len(subdir_dirnames) == 0
    assert subdir_filenames == ["file2"]

    dir_walk = list(bchfs.walk("dir"))
    _, dir_dirnames, dir_filenames = dir_walk[0]
    dir_dirnames = [str(d) for d in dir_dirnames]
    assert dir_dirnames == ["subdir"]
    assert len(dir_filenames) == 0

    assert dir_walk[1] == subdir_walk[0]


def test_cursor___iter__(bchfs: bch.Bcachefs, dir_cursor: bch.Cursor):
    dir_content = []
    for _, d, f in bchfs.walk(dir_cursor.pwd):
        dir_content.extend(d)
        dir_content.extend(f)
    assert sorted(dir_cursor, key=lambda ent: ent.inode) == sorted(
        dir_content,
        key=lambda ent: ent.inode,
    )
    assert sorted(
        [ent for ent in dir_cursor.cd("/")], key=lambda ent: ent.inode
    ) == sorted(set(bchfs), key=lambda ent: ent.inode)


@pytest.mark.images_only([MINI])
def test_namelist(bchfs: bch.Bcachefs):
    assert sorted(bchfs.namelist()) == [
        "dir/subdir/file2",
        "file1",
        "n02033041/n02033041_3834.JPEG",
        "n02445715/n02445715_16523.JPEG",
        "n04467665/n04467665_63788.JPEG",
        "n04584207/n04584207_7936.JPEG",
        "n09332890/n09332890_29876.JPEG",
    ]


def _count_size(fs, name):
    import coverage

    coverage.process_startup()

    try:
        with fs.open(name, "rb") as f:
            return len(f.read())
    except FileNotFoundError:
        return 0


def test_multiprocess(bchfs):
    files = bchfs.namelist()

    with mp.Pool(4) as p:
        sizes = p.starmap(_count_size, [(bchfs, n) for n in files])

    assert sum(sizes) > 1
