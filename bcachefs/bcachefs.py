# This Python file uses the following encoding: utf-8

import io
import os
from dataclasses import dataclass

import numpy as np

from bcachefs.c_bcachefs import PyBcachefs as _Bcachefs, \
    PyBcachefs_iterator as _Bcachefs_iterator

EXTENT_TYPE = 0
DIRENT_TYPE = 2

DIR_TYPE = 4
FILE_TYPE = 8


@dataclass
class Extent:
    inode: int = 0
    file_offset: int = 0
    offset: int = 0
    size: int = 0


@dataclass
class DirEnt:
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

    def __str__(self)
        return self.name


ROOT_DIRENT = DirEnt(0, 4096, DIR_TYPE, '/')
LOSTFOUND_DIRENT = DirEnt(4096, 4097, DIR_TYPE, "lost+found")


class _BCacheFSFileBinary(io.BufferedIOBase):
    """Python file interface for BCachefs files"""
    def __init__(self, name, extents, file, inode, size):
        self.name = file
        self._extents = extents
        self._extend_pos = 0
        self._file = file
        self._buffer = bytearray(8 * 512)
        self._partial = False
        self._inode = inode
        self._pos = 0
        self._size = size

    def close(self):
        self._extents = []
        self._partial = None

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        pass

    @property
    def closed(self) -> bool:
        return len(self._extents) == 0 and not self._partial

    def fileno(self) -> int:
        return self._inode

    def read1(self, size: int) -> bytes:
        """Read at most size bytes with at most one call to the underlying stream"""
        buffer = bytearray(size)
        size = readinto1(b)
        return buffer[:size].data

    def readall(self) -> bytes:
        """Most efficient way to read a file, single allocation"""
        buffer = bytearray(self._size)
        data = []

        s = 0
        e = 0

        memory = memoryview(buffer)

        for extent in self._extents:
            s = e
            e = s + extent.size

            self._file.seek(extent.offset)
            self._file.readinto(memory[s:e])

        return buffer

    def readinto1(self, b: memoryview) -> int:
        """Read at most one extend"""
        # finish reading a block if b was < block size
        # or we had inline data
        if self._partial:
            n = len(self._buffer)
            b[0:n] = self._buffer
            self._partial = False
            self._pos += n
            return n

        if len(self._extents) >= self._extend_pos:
            self.closed = self._closed()
            return 0

        extend = self._extents[self._extend_pos]
        self._extend_pos += 1

        self._file.seek(extent.offset)
        self._file.readinto(self._buffer)

        n = len(b)
        if n < len(self._buffer):
            b[0:n] = self._buffer[:n]
            self._buffer = self._buffer[n:]
            self._partial = True
            return n

        n = len(self._buffer)
        b[:n] = self._buffer
        self._partial = False
        self._pos += n
        return n

    def readinto(self, b: memoryview) -> int:
        return self.readinto1(b)

    def isatty(self):
        return False

    @property
    def readable(self):
        return not self.closed()

    @property
    def seekable(self):
        return False

    def seek(self, offset, whence=io.SEEK_SET):
        raise io.UnsupportedOperation

    def tell(self):
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


class BCacheFS:
    """Open a BCacheFS image for reading

    Examples
    --------
    >>> with BCacheFS('/path/to/image', 'r') as image:

    ...     with image.open('file.txt', 'r') as f:
    ...         string = f.read()

    ...     with image.open('file.bin', 'rb') as f:
    ...         bytes = f.read()

    """
    def __init__(self, path: str, mode: str = 'r'):
        assert mode == 'r', 'Only reading is supported'

        self._path = path
        self._filesystem = None
        self._size = 0
        self._file: [io.RawIOBase] = None
        self._closed = True
        self._pwd = '/'             # Used in Cursor
        self._dirent = ROOT_DIRENT  # Used in Cursor
        self._extents_map = {}
        self._inodes_ls = {ROOT_DIRENT.inode: []}
        self._inodes_tree = {}
        self._open()

    def open(self, name: [str, int], mode: str = 'rb', encoding: str = 'utf-8'):
        """Open a file inside the image for reading

        Parameters
        ----------
        name: str, int
            Path to a file or inode integer

        mode: str
            reading mode rb (bytes) or r (string)

        encoding: str
            string encoding to use, defaults to utf-8
        """
        inode = name
        if isinstance(name, str):
            inode = self.find_dirent(name).inode

        extents = self._extents_map.get(inode)

        if extents is None:
            raise FileNotFoundError(f'{name} was not found')

        file_size = self._inode_map[inode]

        base =_BCacheFSFileBinary(name, extents, self._file, inode, file_size)

        if 'b' in mode:
            return base

        return io.TextIOWrapper(base, encoding)

    def namelist(self):
        """Returns a list of files contained by this archive

        Notes
        -----
        Added for parity with Zipfile interface

        """
        files = []
        for dirent in BcachefsIterDirEnt(self._filesystem):
            files.append(dirent)
        return files

    def __enter__(self):
        return self

    def __exit__(self, _type, _value, _traceback):
        self.close()

    def __iter__(self):
        return (ent for ent in self._inodes_tree.values())

    @property
    def path(self) -> str:
        return self._path

    @property
    def size(self) -> int:
        return self._size

    @property
    def closed(self) -> bool:
        return self._closed

    def cd(self, path: str = '/'):
        cursor = Cursor(self.path, self._extents_map, self._inodes_ls,
                        self._inodes_tree)
        return cursor.cd(path)

    def _open(self):
        if self._closed:
            self._filesystem = _Bcachefs()
            self._filesystem.open(self._path)
            self._size = self._filesystem.size
            self._file = open(self._path, "rb")
            self._closed = False
            self._parse()

    def close(self):
        if not self._closed:
            self._filesystem.close()
            self._filesystem = None
            self._size = 0
            self._file.close()
            self._file = None
            self._closed = True

    def find_dirent(self, path: str = None) -> DirEnt:
        if not path:
            dirent = self._dirent
        else:
            parts = [p for p in path.split('/') if p]
            dirent = self._dirent if not path.startswith("/") else ROOT_DIRENT
            while parts:
                dirent = self._inodes_tree.get((dirent.inode, parts.pop(0)),
                                               None)
                if dirent is None:
                    break
        return dirent

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

    def read_file(self, inode: [str, int]) -> memoryview:
        with self.open(inode) as f:
            return f.readall()

    def walk(self, top: str = None):
        if not top:
            top = self._pwd
            parent = self._dirent
        else:
            top = os.path.join(self._pwd, top)
            parent = self.find_dirent(top)
        if parent:
            return self._walk(top, parent)

    def _parse(self):
        if self._extents_map:
            return

        for dirent in BcachefsIterDirEnt(self._filesystem):
            if dirent.is_dir:
                self._inodes_ls.setdefault(dirent.inode, [])

        for dirent in BcachefsIterDirEnt(self._filesystem):
            self._inodes_ls[dirent.parent_inode].append(dirent)
            self._inodes_tree[(dirent.parent_inode, dirent.name)] = dirent

        for extent in BcachefsIterExtent(self._filesystem):
            self._extents_map.setdefault(extent.inode, [])
            self._extents_map[extent.inode].append(extent)

        for parent_inode, ls in self._inodes_ls.items():
            self._inodes_ls[parent_inode] = self._unique_dirent_list(ls)

    def _walk(self, dirpath: str, dirent: DirEnt):
        dirs = [ent for ent in self._inodes_ls[dirent.inode]
                if ent.is_dir]
        files = [ent for ent in self._inodes_ls[dirent.inode]
                 if not ent.is_dir]
        yield dirpath, dirs, files
        for d in dirs:
            for _ in self._walk(os.path.join(dirpath, d.name), d):
                yield _

    @staticmethod
    def _unique_dirent_list(dirent_ls):
        # It's possible to have multiple inodes for a single file and this
        # implemetation assumes that the last inode should be the correct one.
        return list({ent.name: ent for ent in dirent_ls}.values())


class Cursor(Bcachefs):
    def __init__(self, path: [str, Bcachefs], extents_map: dict,
                 inodes_ls: dict, inodes_tree: dict):
        if isinstance(path, str):
            super(Cursor, self).__init__(path)
        else:
            path: Bcachefs
            super(Cursor, self).__init__(path.path)
        self._extents_map = extents_map
        self._inodes_ls = inodes_ls
        self._inodes_tree = inodes_tree
        self._is_owner = False

    def __iter__(self):
        for _, dirs, files in self.walk():
            for d in dirs:
                yield d
            for f in files:
                yield f

    @property
    def pwd(self):
        return self._pwd

    def cd(self, path: str = '/'):
        if not path:
            path = '/'
            _path = path
        elif path.startswith(".."):
            pwd = self._pwd.split('/')
            path = path.split('/')
            while pwd and path and path[0] == "..":
                pwd.pop()
                path.pop(0)
            pwd = '/'.join(pwd)
            if not pwd:
                pwd = '/'
            path = os.path.join(pwd, *path)
            _path = path
        else:
            _path = os.path.join(self._pwd, path)
        dirent = self.find_dirent(path)
        if dirent and dirent.is_dir:
            self._pwd = _path
            self._dirent = dirent
            return self
        else:
            return None


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


class BcachefsIterExtent(BcachefsIter):
    def __init__(self, fs: _Bcachefs):
        super(BcachefsIterExtent, self).__init__(fs, EXTENT_TYPE)

    def __next__(self):
        return Extent(*super(BcachefsIterExtent, self).__next__())


class BcachefsIterDirEnt(BcachefsIter):
    def __init__(self, fs: _Bcachefs):
        super(BcachefsIterDirEnt, self).__init__(fs, DIRENT_TYPE)

    def __next__(self):
        return DirEnt(*super(BcachefsIterDirEnt, self).__next__())
