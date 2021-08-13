# This Python file uses the following encoding: utf-8

from dataclasses import dataclass
import io
import os
import typing

import numpy as np

from benzina.c_bcachefs import PyBCacheFS as _BCacheFS, \
    PyBCacheFS_iterator as _BCacheFS_iterator

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


ROOT_DIRENT = DirEnt(0, 4096, DIR_TYPE, '/')


class BCacheFS:
    def __init__(self, path: str):
        self._path = path
        self._filesystem = _BCacheFS()
        self._closed = True
        self._extents_map = {}
        self._inodes_ls = {ROOT_DIRENT.inode: []}
        self._inodes_tree = {}

    def __iter__(self):
        self.open()
        return BCacheFSIterDirEnt(self._filesystem)

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, _type, _value, _traceback):
        self.close()

    @property
    def path(self) -> str:
        return self._path

    @property
    def size(self) -> int:
        return self._filesystem.size

    @property
    def closed(self) -> bool:
        return self._closed

    def open(self):
        if self._closed:
            self._filesystem.open(self._path)
            self._closed = False
            self._parse()

    def close(self):
        if not self._closed:
            self._filesystem.close()
            self._closed = True

    def find_dirent(self, path: str = '/'):
        parts = [p for p in path.split('/') if p] if path else None
        dirent = ROOT_DIRENT
        while parts:
            dirent = self._inodes_tree.get((dirent.inode, parts.pop(0)), None)
            if dirent is None:
                break
        return dirent

    def ls(self, path: [str, DirEnt] = '/'):
        if isinstance(path, DirEnt):
            parent = path
        else:
            parent = self.find_dirent(path)
        if parent.is_dir:
            return self._inodes_ls[parent.inode]
        else:
            return [parent]

    def read_file(self, fs: [typing.BinaryIO, io.RawIOBase],
                  inode: [str, int]) -> np.array:
        if isinstance(inode, str):
            inode = self.find_dirent(inode).inode
        extents = self._extents_map[inode]
        file_size = 0
        for extent in extents:
            file_size += extent.size
        _bytes = np.empty(file_size, dtype="<u1")
        for extent in extents:
            fs.seek(extent.offset)
            fs.readinto(_bytes[extent.file_offset:
                               extent.file_offset+extent.size])
        return _bytes

    def walk(self, top: str = '/', dirent: DirEnt = None):
        if dirent and dirent.name == os.path.basename(top):
            parent = dirent
        else:
            parent = self.find_dirent(top)
        if parent:
            return self._walk(top, parent)

    def _parse(self):
        if self._extents_map:
            return

        for dirent in BCacheFSIterDirEnt(self._filesystem):
            self._inodes_ls[dirent.parent_inode].append(dirent)
            if dirent.is_dir:
                self._inodes_ls[dirent.inode] = []
            self._inodes_tree[(dirent.parent_inode, dirent.name)] = dirent

        for extent in BCacheFSIterExtent(self._filesystem):
            self._extents_map.setdefault(extent.inode, [])
            self._extents_map[extent.inode].append(extent)

    def _walk(self, dirpath: str, dirent: DirEnt):
        dirs = [de for de in self._inodes_ls[dirent.inode]
                if de.is_dir]
        files = [de for de in self._inodes_ls[dirent.inode]
                 if not de.is_dir]
        yield dirpath, dirs, files
        for d in dirs:
            for _ in self._walk(os.path.join(dirpath, d.name), d):
                yield _


class Cursor:
    def __init__(self, path: [str, BCacheFS]):
        if isinstance(path, str):
            self._bchfs: BCacheFS = BCacheFS(path)
        else:
            self._bchfs: BCacheFS = path
        self._is_owner = False
        self._pwd = '/'
        self._dirent = None
        with self:
            self._dirent = self._bchfs.find_dirent(self.pwd)

    def __iter__(self):
        return iter(self._bchfs)

    def __enter__(self):
        self._is_owner = self._bchfs.closed
        self._bchfs.open()
        return self

    def __exit__(self, _type, _value, _traceback):
        if self._is_owner:
            self._bchfs.close()

    @property
    def bchfs(self):
        return self._bchfs

    @property
    def pwd(self):
        return self._pwd

    def cd(self, path: str = '/'):
        if not path:
            path = '/'
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
        elif path[0] != '/':
            path = os.path.join(self._pwd, path)
        dirent = self._bchfs.find_dirent(path)
        if dirent and dirent.is_dir:
            self._pwd = path
            self._dirent = dirent

    def ls(self):
        return self._bchfs.ls(self._dirent)

    def walk(self):
        return self._bchfs.walk(self._pwd, self._dirent)


class BCacheFSIter:
    def __init__(self, fs: _BCacheFS, t: int = DIRENT_TYPE):
        self._iter: _BCacheFS_iterator = fs.iter(t)

    def __iter__(self):
        return self

    def __next__(self):
        item = self._iter.next()
        if item is None:
            raise StopIteration
        return item


class BCacheFSIterExtent(BCacheFSIter):
    def __init__(self, fs: _BCacheFS):
        super(BCacheFSIterExtent, self).__init__(fs, EXTENT_TYPE)

    def __next__(self):
        return Extent(*super(BCacheFSIterExtent, self).__next__())


class BCacheFSIterDirEnt(BCacheFSIter):
    def __init__(self, fs: _BCacheFS):
        super(BCacheFSIterDirEnt, self).__init__(fs, DIRENT_TYPE)

    def __next__(self):
        return DirEnt(*super(BCacheFSIterDirEnt, self).__next__())
