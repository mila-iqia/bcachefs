# This Python file uses the following encoding: utf-8

import io
import os
from dataclasses import dataclass

import numpy as np

from bcachefs.c_bcachefs import (
    PyBcachefs as _Bcachefs,
    PyBcachefs_iterator as _Bcachefs_iterator,
)


EXTENT_TYPE = 0
INODE_TYPE = 1
DIRENT_TYPE = 2

DIR_TYPE = 4
FILE_TYPE = 8


@dataclass(eq=True, frozen=True)
class Extent:
    """Specify the location of an extent of a file inside the disk image

    Attributes
    ----------
    inode: int
        inode of the file

    file_offset: int
        position of the extent in the logical file

    offset: int
        position inside the disk image where the extent starts

    size: int
        size of the extent

    """

    inode: int = 0
    file_offset: int = 0
    offset: int = 0
    size: int = 0


@dataclass(eq=True, frozen=True)
class Inode:
    """Bcachefs Inode Attributes

    Attributes
    ----------
    inode: int
        inode the attributes belongs to

    size: int
        file size

    """

    inode: int = 0
    size: int = 0
    hash_seed: int = 0


@dataclass(eq=True, frozen=True)
class DirEnt:
    """Bcachefs directory entry

    Attributes
    ----------
    parent_inode: int
        inode of the parent entry (directory)

    inode: int
        inode of the current entry

    type: int
        file (8) or directory (4)

    name: str
        name of current entry (file or directory)

    """

    parent_inode: int = 0
    inode: int = 0
    type: int = 0
    name: str = ""

    @property
    def is_dir(self):
        return self.type == DIR_TYPE

    @property
    def is_file(self):
        return self.type == FILE_TYPE

    def __str__(self):
        return self.name


ROOT_DIRENT = DirEnt(4096, 4096, DIR_TYPE, "")
LOSTFOUND_DIRENT = DirEnt(4096, 4097, DIR_TYPE, "lost+found")


class _BcachefsFileBinary(io.BufferedIOBase):
    """Python file interface for Bcachefs files

    Parameters
    ----------
    name: str
        name of the file being opened

    extends
        list of Extent

    file: file object
        underlying opened disk image file

    inode: int
        inode of the file being opened

    size: int
        size of the file being opened
    """

    def __init__(self, name, extents, file, inode, size):
        self.name = file
        self._inode = inode
        self._size = size

        # underlying bcachefs archive
        # DO NOT close this!!
        self._file = file

        # sort by offset so the extents are always in the right order
        sorted(extents, key=lambda extent: extent.file_offset)
        self._extents = extents

        self._extent_pos = 0  # current extent being read
        self._extent_read = (
            0  # offset pointing to the unread part of the current extend
        )
        self._pos = 0  # absolute position inside the file

    def reset(self):
        """Reset internal state to point to the begining of the file"""
        self._extent_pos = 0
        self._extend_read = 0
        self._pos = 0

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        pass

    @property
    def closed(self) -> bool:
        """return true if we finished reading the current file

        Notes
        -----
        You can reuse the same file multiple time by calling `reset`

        """
        return self._extent_pos >= len(self._extents)

    def fileno(self) -> int:
        """returns the inode of the file inside bcachefs"""
        return self._inode

    def read(self, n=-1) -> bytes:
        """Read at most n bytes

        Parameters
        ----------
        n: int
            max size that can be read if -1 all the file is read

        """
        if n == -1:
            return self.readall()

        buffer = np.empty(n, dtype="<u1")
        view = memoryview(buffer)
        size = self.readinto(view)
        return bytes(buffer[:size])

    def read1(self, size: int) -> bytes:
        """Read at most size bytes with at most one call to the underlying stream"""
        buffer = np.empty(size, dtype="<u1")
        view = memoryview(buffer)
        size = self.readinto1(view)
        return bytes(buffer[:size])

    def readall(self) -> bytes:
        """Most efficient way to read a file, single allocation"""
        buffer = np.empty(self._size, dtype="<u1")
        memory = memoryview(buffer)

        for extent in self._extents:
            s = extent.file_offset
            e = s + extent.size

            self._file.seek(extent.offset)
            self._file.readinto(memory[s:e])

        return bytes(buffer)

    def readinto1(self, b: memoryview) -> int:
        """Read at most one extend

        Notes
        -----
        The size of the buffer is not checked against the extent size,
        this means we could possibly read beyond the extent but the size returned
        will be inside the bounds.

        """

        # we ran out of extent, done
        if self._extent_pos >= len(self._extents):
            return 0

        # continue reading the current extent
        extent = self._extents[self._extent_pos]

        self._file.seek(extent.offset + self._extent_read)
        read = self._file.readinto(b)

        self._extent_read += read
        self._pos += read

        # if we finished reading current extend go the the next one
        if self._extent_read >= extent.size:
            self._extent_pos += 1
            self._extent_read = 0

        # finished reading the file
        if self._pos > self._size:
            diff = self._pos - self._size

            self._extent_pos += 1
            self._extent_read = 0
            return read - diff

        return read

    def readinto(self, b: memoryview) -> int:
        """Read until the buffer is full"""
        n = len(b)
        size = self.readinto1(b)

        while size < n and not self.closed:
            size += self.readinto1(b[size:])

        return size

    @property
    def isatty(self):
        return False

    @property
    def readable(self):
        return not self.closed

    @property
    def seekable(self):
        return True

    def seek(self, offset, whence=io.SEEK_SET):
        """Seek a specific position inside the file"""
        if whence == io.SEEK_END:
            return self.seek(self._size + offset, io.SEEK_SET)

        if whence == io.SEEK_CUR:
            return self.seek(self._pos + offset, io.SEEK_SET)

        if whence == io.SEEK_SET:
            self.reset()

            e = 0
            for i, extent in enumerate(self._extents):
                s = extent.file_offset
                e = s + extent.size

                if s <= offset < e:
                    self._extent_pos = i
                    self._extent_read = offset - s
                    self._pos = offset
                    break

            return offset

    def tell(self):
        """Returns the current possition of the file cursor"""
        return self._pos

    def detach(self):
        raise io.UnsupportedOperation

    @property
    def writable(self):
        return False

    def writelines(self, lines):
        raise io.UnsupportedOperation

    def write(self, b):
        raise io.UnsupportedOperation

    def flush(self):
        pass


class Bcachefs:
    """Opens a Bcachefs disk image for reading

    Parameters
    ----------
    path: str
        path to the disk image

    Examples
    --------
    >>> with Bcachefs(path_to_file, 'r') as image:
    ...     with image.open('dir/subdir/file2', 'rb') as f:
    ...         data = f.read()
    ...         print(data.decode('utf-8'))
    File content 2
    <BLANKLINE>

    """

    def __init__(self, path: str, mode: str = "rb"):
        assert mode in ("r", "rb"), "Only reading is supported"
        self._path = path
        self._filesystem = None
        self._file: [io.RawIOBase] = None
        self._closed = True
        self._open()

    def open(self, name: [str, int], mode: str = "rb", encoding: str = "utf-8"):
        """Open a file inside the image for reading

        Parameters
        ----------
        name: str, int
            Path to a file or inode integer

        mode: str
            reading mode rb (bytes)

        encoding: str
            string encoding to use, defaults to utf-8

        Raises
        ------
        FileNotFoundError when opening an file that does not exist

        """
        inode = name
        if isinstance(name, str):
            dirent = self.find_dirent(name)
            inode = dirent.inode if dirent else None

        inode = self.find_inode(inode)

        extents = list(self.find_extents(inode.inode if inode else None))

        if not extents:
            raise FileNotFoundError(f"{name} was not found")

        file_size = inode.size
        base = _BcachefsFileBinary(
            name, extents, self._file, inode.inode, file_size
        )
        return base

    def namelist(self):
        """Returns a list of files contained by this archive

        Notes
        -----
        Added for parity with Zipfile interface

        Examples
        --------

        >>> with Bcachefs(path_to_file, 'r') as image:
        ...     print(image.namelist())
        ['file1', 'n09332890/n09332890_29876.JPEG', 'dir/subdir/file2', 'n04467665/n04467665_63788.JPEG', 'n02033041/n02033041_3834.JPEG', 'n02445715/n02445715_16523.JPEG', 'n04584207/n04584207_7936.JPEG']

        """

        for root, _, files in self.walk():
            for f in files:
                yield os.path.join(root, f.name)

    def __enter__(self):
        return self

    def __exit__(self, _type, _value, _traceback):
        self.close()

    def __iter__(self):
        return BcachefsIterDirEnt(self._filesystem)

    @property
    def path(self) -> str:
        """Path of the current image"""
        return self._path

    @property
    def size(self) -> int:
        return self._filesystem.size if self._filesystem else None

    @property
    def closed(self) -> bool:
        """Is current image closed"""
        return self._closed

    def cd(self, path: str = "/"):
        """Creates a cursor to a directory"""
        return Cursor(self, path)

    def _open(self):
        if self._closed:
            self._filesystem = _Bcachefs()
            self._filesystem.open(self._path)
            self._file = open(self._path, "rb")
            self._closed = False

    def close(self):
        if not self._closed:
            self._filesystem.close()
            self._filesystem = None
            self._file.close()
            self._file = None
            self._closed = True

    def extents(self):
        for extent in BcachefsIterExtent(self._filesystem):
            yield extent

    def inodes(self):
        for inode in BcachefsIterInode(self._filesystem):
            yield inode

    def dirents(self):
        for dirent in BcachefsIterDirEnt(self._filesystem):
            yield dirent

    def find_extent(self, inode: int, file_offset: int) -> Extent:
        extent = self._filesystem.find_extent(inode, file_offset)
        return Extent(*extent) if extent else None

    def find_extents(self, inode: int) -> Extent:
        extent = self.find_extent(inode, 0)
        while extent:
            yield extent
            extent = self.find_extent(inode, extent.file_offset + extent.size)

    def find_inode(self, inode: int) -> Inode:
        inode = self._filesystem.find_inode(inode)
        return Inode(*inode) if inode else None

    def find_dirent(self, path: [bytes, str] = None) -> DirEnt:
        """Resolve a path to its directory entry, returns none if it was not found"""
        dirent = ROOT_DIRENT
        if path:
            if isinstance(path, str):
                path = path.encode()
            parts = [p for p in path.split(b"/") if p]
            while parts:
                dirent = self._filesystem.find_dirent(
                    dirent.inode, 0, parts.pop(0)
                )
                if dirent is None:
                    break
                else:
                    dirent = DirEnt(*dirent)
        return dirent

    def find_dirents(self, path: [DirEnt, bytes, str] = None) -> DirEnt:
        if isinstance(path, DirEnt):
            root = path
        else:
            root = self.find_dirent(path)

        if root is None:
            raise StopIteration
        if root.type == DIR_TYPE:
            iter = BcachefsIterDirEnt(self._filesystem)
            for dirent in iter:
                if dirent.parent_inode == root.inode:
                    yield dirent
                elif dirent.parent_inode > root.inode:
                    iter.next_bset()
        else:
            yield root

    def ls(self, path: [str, DirEnt] = None):
        """Show the files inside a given directory"""
        if isinstance(path, DirEnt):
            parent = path
        elif not path:
            parent = ROOT_DIRENT
        else:
            parent = self.find_dirent(path)
        if parent.is_dir:
            for dirent in self.find_dirents(parent):
                yield dirent
        else:
            yield parent

    def read_file(self, inode: [str, int]) -> memoryview:
        with self.open(inode) as f:
            return f.readall()

    def walk(self, top: str = None):
        """Traverse the file system recursively"""
        if not top:
            parent = ROOT_DIRENT
            top = parent.name
        elif isinstance(top, DirEnt):
            parent = top
            top = top.name
        else:
            parent = self.find_dirent(top)
        if parent:
            return self._walk(top, parent)

    def _walk(self, top: str, dirent: DirEnt):
        ls = list(self.ls(dirent))
        dirs = [ent for ent in ls if ent.is_dir]
        files = [ent for ent in ls if not ent.is_dir]
        yield top, dirs, files
        for d in dirs:
            yield from self._walk(os.path.join(top, d.name), d)

    def __getstate__(self):
        state = self.__dict__.copy()
        del state["_file"]
        del state["_filesystem"]
        return state

    def __setstate__(self, state):
        self.__dict__ = {**self.__dict__, **state}
        if not self._closed:
            self._filesystem = _Bcachefs()
            self._filesystem.open(self._path)
            self._file = open(self._path, "rb")


class Cursor:
    def __init__(
        self,
        filesystem: [str, Bcachefs],
        path: [str, Bcachefs],
    ):
        with (
            Bcachefs(filesystem) if isinstance(filesystem, str) else filesystem
        ) as fs:
            self._file = open(fs.path, "rb")
            self._pwd = path
            self._dirent = fs.find_dirent(path)
            self._extents_map = {}
            self._inodes_ls = {self._dirent.inode: []}
            self._inodes_tree = {}
            self._inode_map = {}
            self._parse(fs)

    def __enter__(self):
        if self._file.closed:
            self._file = open(self._file.name, "rb")
        return self

    def __exit__(self, _type, _value, _traceback):
        self.close()

    def __iter__(self):
        for _, dirs, files in self.walk():
            for d in dirs:
                yield d
            for f in files:
                yield f

    def __getstate__(self):
        state = self.__dict__.copy()
        state["_file"] = self._file.name
        return state

    def __setstate__(self, state):
        self.__dict__ = {**self.__dict__, **state}
        self._file = open(self._file, "rb")

    @property
    def filename(self) -> str:
        return self._path

    @property
    def size(self) -> int:
        return self._size

    @property
    def closed(self) -> bool:
        return self._closed

    @property
    def pwd(self) -> str:
        return self._pwd

    def close(self):
        if not self._file.closed:
            self._file.close()

    def ls(self, path: [str, DirEnt] = None):
        """Show the files inside a given directory"""
        if isinstance(path, DirEnt):
            parent = path
        elif not path:
            parent = self._dirent
        else:
            parent = self.find_dirent(os.path.join(self._pwd, path))
        if parent.is_dir:
            return self._inodes_ls[parent.inode]
        else:
            return [parent]

    def walk(self, top: str = None):
        if not top:
            parent = self._dirent
            top = self._pwd
        elif isinstance(top, DirEnt):
            parent = top
            top = top.name
        else:
            parent = self.find_dirent(top)
            top = os.path.join(self._pwd, top)
        if parent:
            return self._walk(top, parent)

    def open(self, name: [str, int], mode: str = "rb", encoding: str = "utf-8"):
        inode = name
        if isinstance(name, str):
            inode = self.find_dirent(name).inode

        extents = self._extents_map.get(inode)

        if extents is None:
            raise FileNotFoundError(f"{name} was not found")

        file_size = self._inode_map[inode]
        base = _BcachefsFileBinary(name, extents, self._file, inode, file_size)
        return base

    def read(self, inode: [str, int]) -> memoryview:
        with self.open(inode) as f:
            return f.readall()

    def namelist(self):
        for root, _, files in self.walk():
            for f in files:
                yield os.path.join(root, f.name)

    def find_dirent(self, path: str = None) -> DirEnt:
        dirent = self._dirent
        if path:
            parts = [p for p in path.split("/") if p]
            while parts:
                dirent = self._inodes_tree.get(
                    (dirent.inode, parts.pop(0)), None
                )
                if dirent is None:
                    break
        return dirent

    def _parse(self, filesystem: Bcachefs):
        """Generate a cache of bcachefs btrees"""
        if self._extents_map:
            return

        for _, dirs, files in filesystem.walk(self._dirent):
            for d in dirs:
                self._inodes_ls.setdefault(d.inode, [])
                self._inodes_ls[d.parent_inode].append(d)
                self._inodes_tree[(d.parent_inode, d.name)] = d
            for f in files:
                self._extents_map[f.inode] = []
                self._inode_map[f.inode] = None
                self._inodes_ls[f.parent_inode].append(f)
                self._inodes_tree[(f.parent_inode, f.name)] = f

        for extent in filesystem.extents():
            if extent.inode not in self._extents_map:
                continue
            self._extents_map[extent.inode].append(extent)

        for inode in filesystem.inodes():
            if inode.inode not in self._inode_map:
                continue
            self._inode_map[inode.inode] = inode

    def _walk(self, top: str, dirent: DirEnt):
        dirs = [ent for ent in self._inodes_ls[dirent.inode] if ent.is_dir]
        files = [ent for ent in self._inodes_ls[dirent.inode] if ent.is_file]
        yield top, dirs, files
        for d in dirs:
            yield from self._walk(os.path.join(top, d.name), d)


class BcachefsIter:
    def __init__(self, fs: _Bcachefs, t: int = DIRENT_TYPE):
        self._iter: _Bcachefs_iterator = fs.iter(t)

    def __iter__(self):
        return self

    def __next__(self):
        item = self._iter.next()
        if item is None:
            raise StopIteration
        return item

    def next_bset(self):
        return self._iter.next_bset()


class BcachefsIterExtent(BcachefsIter):
    """Iterates over bcachefs extend btree"""

    def __init__(self, fs: _Bcachefs):
        super(BcachefsIterExtent, self).__init__(fs, EXTENT_TYPE)

    def __next__(self):
        return Extent(*super(BcachefsIterExtent, self).__next__())


class BcachefsIterInode(BcachefsIter):
    """Iterates over bcachefs inode btree"""

    def __init__(self, fs: _Bcachefs):
        super(BcachefsIterInode, self).__init__(fs, INODE_TYPE)
        self._deleted = set()

    def __next__(self):
        inode = Inode(*super(BcachefsIterInode, self).__next__())
        while not inode.hash_seed and inode.inode not in self._deleted:
            self._deleted.add(inode.inode)
            inode = Inode(*super(BcachefsIterInode, self).__next__())
        return inode


class BcachefsIterDirEnt(BcachefsIter):
    """Iterates over bcachefs dirent btree"""

    def __init__(self, fs: _Bcachefs):
        super(BcachefsIterDirEnt, self).__init__(fs, DIRENT_TYPE)
        self._deleted = set()

    def __next__(self):
        dirent = DirEnt(*super(BcachefsIterDirEnt, self).__next__())
        while (
            not dirent.inode
            and (dirent.parent_inode, dirent.name) not in self._deleted
        ):
            self._deleted.add((dirent.parent_inode, dirent.name))
            dirent = DirEnt(*super(BcachefsIterDirEnt, self).__next__())
        return dirent
