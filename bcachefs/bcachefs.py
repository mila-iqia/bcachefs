# This Python file uses the following encoding: utf-8

import io
import os
from dataclasses import dataclass
from typing import Generator, List, Tuple, Union

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
        del args, kwargs
        pass

    @property
    def closed(self) -> bool:
        """Return true if we finished reading the current file

        Notes
        -----
        You can reuse the same file multiple time by calling `reset`
        """
        return self._extent_pos >= len(self._extents)

    def fileno(self) -> int:
        """Returns the inode of the file inside bcachefs"""
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
        will be inside the bounds
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
        del lines
        raise io.UnsupportedOperation

    def write(self, b):
        raise io.UnsupportedOperation

    def flush(self):
        pass


class FilesystemMixin:
    def __init__(self):
        self._file = None

    def __enter__(self):
        self.mount()
        return self

    def __exit__(self, type, value, traceback):
        del type, value, traceback
        self.umount()

    def __iter__(self) -> Generator[DirEnt, None, None]:
        raise NotImplemented

    @property
    def filename(self) -> str:
        """Path of the current disk image"""
        raise NotImplemented

    @property
    def unmounted(self) -> bool:
        """Is current disk image unmounted"""
        raise NotImplemented

    def cd(self, path: Union[str, DirEnt]):
        """Open a cursor to specified directory and cache its content

        Parameters
        ----------
        path: str, DirEnt
            Path or DirEnt of a file or directory
        """
        return Cursor(self, path)

    def open(
        self, name: Union[str, int], mode: str = "rb", encoding: str = "utf-8"
    ):
        """Open a file and return the corresponding file object

        Parameters
        ----------
        name: str, int
            Path or inode integer of a file

        mode: str
            reading mode rb (bytes)

        encoding: str
            string encoding to use, defaults to utf-8

        Raises
        ------
        FileNotFoundError when opening an file that does not exist
        """
        del mode, encoding
        inode = name
        if isinstance(name, DirEnt):
            inode = name.inode
        elif isinstance(name, str):
            dirent = self._find_dirent(name)
            inode = dirent.inode if dirent else None

        inode = self._find_inode(inode)

        extents = list(self._find_extents(inode.inode if inode else None))

        if not extents:
            raise FileNotFoundError(f"{name} was not found")

        file_size = inode.size
        base = _BcachefsFileBinary(
            name, extents, self._file, inode.inode, file_size
        )
        return base

    def read(self, inode: Union[str, int]) -> memoryview:
        """Read and return all the bytes from the file

        Parameters
        ----------
        inode: str, int
            Path or inode integer of a file
        """
        with self.open(inode) as f:
            return f.readall()

    def readinto(
        self, inode: Union[str, int], buffer: memoryview
    ) -> memoryview:
        """Read bytes into a pre-allocated, writable bytes-like object b, and
        return the number of bytes read

        Parameters
        ----------
        inode: str, int
            Path or inode integer of a file
        """
        with self.open(inode) as f:
            return f.readinto(buffer)

    def scandir(
        self, path: Union[str, DirEnt] = None
    ) -> Generator[DirEnt, None, None]:
        """Return an iterator of DirEnt objects corresponding to the entries in
        the directory given by path

        Parameters
        ----------
        path: str, DirEnt
            Path or DirEnt of a directory
        """
        if isinstance(path, DirEnt):
            parent = path
        else:
            parent = self._find_dirent(path)

        return self._find_dirents(parent)

    def umount(self):
        """Unmount of the disk image. This invalidates all open files objects

        Notes
        -----
        This in fact closes the disk image file.
        """
        raise NotImplemented

    def walk(self, top: str = None):
        """Generate the dirents in a directory tree by walking the tree
        either top-down. For each directory in the tree rooted at directory top
        (including top itself), it yields a 3-tuple `(dirpath, dirs,
        files)`

        Parameters
        ----------
        top: str, int
            Path or DirEnt of a file
        """
        if isinstance(top, DirEnt):
            parent = top
            top = top.name
        elif not top:
            parent = self._find_dirent(top)
            top = parent.name
        else:
            parent = self._find_dirent(top)
        if parent:
            return self._walk(top, parent)

    def _find_extent(self, inode: int, file_offset: int) -> Extent:
        """Return the extent descriptor of an inode

        The file offset needs to exist in the extents list

        Parameters
        ----------
        inode: int
            inode integer of a file

        file_offset: int
            offset of the extent in the file
        """
        del inode, file_offset
        raise NotImplemented

    def _find_extents(self, inode: int) -> Generator[Extent, None, None]:
        """Return the list of extents descriptors of an inode

        Parameters
        ----------
        inode: int
            inode integer of a file
        """
        del inode
        raise NotImplemented

    def _find_inode(self, inode: int) -> Inode:
        """Return the inode informations of a file

        Parameters
        ----------
        inode: int
            inode integer of a file
        """
        del inode
        raise NotImplemented

    def _find_dirent(self, path: Union[bytes, str] = None) -> DirEnt:
        """Return the dirent informations of a file

        Parameters
        ----------
        path: str
            Path of a file
        """
        del path
        raise NotImplemented

    def _find_dirents(
        self, dirent: DirEnt = None
    ) -> Generator[DirEnt, None, None]:
        """Return the list of dirent in a directory

        Parameters
        ----------
        dirent: DirEnt
            DirEnt of a directory
        """
        del dirent
        raise NotImplemented

    def _walk(self, top: str, dirent: DirEnt):
        del top, dirent
        raise NotImplemented


def mount(file) -> "Bcachefs":
    """Virtually mount a disk image to access its files

    Parameters
    ----------
    file: str
        path to the disk image

    Notes
    -----
    This in fact opens the disk image file for reading operations.

    Examples
    --------
    >>> with mount(path_to_file) as image:
    ...     with image.open('dir/subdir/file2', 'rb') as f:
    ...         data = f.read()
    ...         print(data.decode('utf-8'))
    File content 2
    <BLANKLINE>
    """
    return Bcachefs(file)


class ZipFileLikeMixin(FilesystemMixin):
    """Open a disk image to access its files

    Parameters
    ----------
    file: str
        path to the disk image

    Notes
    -----
    This in fact opens the disk image file for reading operations.

    Examples
    --------
    >>> with Bcachefs(path_to_file) as image:
    ...     with image.open('dir/subdir/file2', 'rb') as f:
    ...         data = f.read()
    ...         print(data.decode('utf-8'))
    File content 2
    <BLANKLINE>
    """

    @property
    def closed(self) -> bool:
        """Is current disk image closed"""
        return self.unmounted

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
            for f in set(files):
                yield os.path.join(root, f.name)

    def open(
        self, name: Union[str, int], mode: str = "rb", encoding: str = "utf-8"
    ):
        return FilesystemMixin.open(self, name, mode, encoding)

    def read(self, inode: Union[str, int]) -> memoryview:
        return FilesystemMixin.read(self, inode)

    def close(self):
        """Close the disk image. This invalidates all open files objects"""
        FilesystemMixin.umount(self)

    def cache_dir(self, path: Union[str, DirEnt]):
        """Open a cursor to specified directory and cache its content

        Parameters
        ----------
        path: str, DirEnt
            Path or DirEnt of a file or directory
        """
        return self.cd(path)


class Bcachefs(ZipFileLikeMixin):
    def __init__(self, path: str, mode: str = "rb"):
        assert mode in ("r", "rb"), "Only reading is supported"
        self._filesystem = _Bcachefs()
        self._filesystem.open(path)
        self._file: io.RawIOBase = open(path, "rb")
        self._unmounted = False

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        del type, value, traceback
        self.umount()

    def __iter__(self):
        return BcachefsIterDirEnt(self._filesystem)

    def __getstate__(self):
        state = self.__dict__.copy()
        state["_file"] = self._file.name
        del state["_filesystem"]
        return state

    def __setstate__(self, state):
        self.__dict__ = {**self.__dict__, **state}
        self._file = open(self._file, "rb")
        if self._unmounted:
            self._filesystem = None
            self._file.close()
        else:
            self._filesystem = _Bcachefs()
            self._filesystem.open(self._file.name)

    @property
    def filename(self) -> str:
        return self._file.name

    @property
    def unmounted(self) -> bool:
        return self._unmounted

    def cd(self, path: str = ""):
        return Cursor(self, path)

    def extents(self):
        for extent in BcachefsIterExtent(self._filesystem):
            yield extent

    def inodes(self):
        for inode in BcachefsIterInode(self._filesystem):
            yield inode

    def dirents(self):
        for dirent in BcachefsIterDirEnt(self._filesystem):
            yield dirent

    def umount(self):
        if not self._unmounted:
            self._filesystem.close()
            self._filesystem = None
            self._file.close()
            self._unmounted = True

    def _find_extent(self, inode: int, file_offset: int) -> Extent:
        extent = (
            self._filesystem.find_extent(inode, file_offset) if inode else None
        )
        return Extent(*extent) if extent else None

    def _find_extents(self, inode: int) -> Generator[Extent, None, None]:
        extent = self._find_extent(inode, 0)
        while extent:
            yield extent
            extent = self._find_extent(inode, extent.file_offset + extent.size)

    def _find_inode(self, inode: int) -> Inode:
        inode = self._filesystem.find_inode(inode)
        return Inode(*inode) if inode else None

    def _find_dirent(self, path: Union[bytes, str] = None) -> DirEnt:
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

    def _find_dirents(
        self, dirent: DirEnt = None
    ) -> Generator[DirEnt, None, None]:
        iter = BcachefsIterDirEnt(self._filesystem)
        for ent in iter:
            if ent.parent_inode == dirent.inode:
                yield ent
            elif ent.parent_inode > dirent.inode:
                iter.next_bset()

    def _walk(self, top: str, dirent: DirEnt):
        ls = set(self.scandir(dirent))
        dirs = [ent for ent in ls if ent.is_dir]
        files = [ent for ent in ls if not ent.is_dir]
        yield top, dirs, files
        for d in dirs:
            yield from self._walk(os.path.join(top, d.name), d)


class Cursor(ZipFileLikeMixin):
    """Cursor of a filesystem opened at a specific directory. Calls will be made
    relative to that directory and its recursive content will be cached"""

    def __init__(
        self,
        filesystem: Union[str, FilesystemMixin],
        path: str,
        extents_map=None,
        inodes_ls=None,
        inodes_tree=None,
        inode_map=None,
    ):
        fs = Bcachefs(filesystem) if isinstance(filesystem, str) else filesystem
        self._file = open(fs.filename, "rb")
        self._pwd = path.strip("/")
        self._dirent = fs._find_dirent(path)
        self._extents_map = extents_map
        self._inodes_ls = inodes_ls
        self._inodes_tree = inodes_tree
        self._inode_map = inode_map
        self._parse(fs)

    def __enter__(self):
        if self._file.closed:
            self._file = open(self._file.name, "rb")
        return self

    def __exit__(self, type, value, traceback):
        del type, value, traceback
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
        return self._file.name

    @property
    def closed(self) -> bool:
        return self._file.closed

    @property
    def pwd(self) -> str:
        return self._pwd

    def cd(self, path: Union[str, int] = ""):
        if not path:
            path = "/"
        if self._find_dirent(path):
            fs = self
            extents_map = self._extents_map
            inodes_ls = self._inodes_ls
            inodes_tree = self._inodes_tree
            inode_map = self._inode_map
        else:
            fs = self.filename
            extents_map = None
            inodes_ls = None
            inodes_tree = None
            inode_map = None

        return Cursor(fs, path, extents_map, inodes_ls, inodes_tree, inode_map)

    def close(self):
        if not self._file.closed:
            self._file.close()

    def _find_extent(self, inode: int, file_offset: int) -> Extent:
        for extent in self._find_extents(inode):
            if extent.file_offset == file_offset:
                return extent
            elif extent.file_offset > file_offset:
                break

    def _find_extents(self, inode: int) -> Generator[Extent, None, None]:
        extents = self._extents_map.get(inode, None)
        if extents is None:
            raise StopIteration
        else:
            for extent in extents:
                yield extent

    def _find_inode(self, inode: int) -> Inode:
        return self._inode_map.get(inode, None)

    def _find_dirent(self, path: str = None) -> DirEnt:
        dirent = ROOT_DIRENT if path and path.startswith("/") else self._dirent
        if (
            dirent is not self._dirent
            and self._inodes_ls.get(dirent.inode, None) is None
        ):
            dirent = None
        elif path:
            parts = [p for p in path.split("/") if p]
            while parts:
                dirent = self._inodes_tree.get(
                    (dirent.inode, parts.pop(0)), None
                )
                if dirent is None:
                    break
        return dirent

    def _find_dirents(self, dirent: DirEnt = None) -> DirEnt:
        for ent in self._inodes_ls[dirent.inode]:
            yield ent

    def _parse(self, filesystem: Bcachefs):
        """Generate a cache of bcachefs btrees"""
        if self._extents_map:
            return

        self._inodes_ls = {ROOT_DIRENT.inode: []}
        self._inodes_tree = {}

        # Keep a clean version of the structs
        extents_map = {}
        inodes_ls = {self._dirent.inode: []}
        inodes_tree = {}
        inode_map = {}

        # Load all dirents
        dirents = list(filesystem.dirents())
        for dirent in dirents:
            if dirent.is_dir:
                self._inodes_ls.setdefault(dirent.inode, [])

        for dirent in dirents:
            self._inodes_ls[dirent.parent_inode].append(dirent)
            self._inodes_tree[(dirent.parent_inode, dirent.name)] = dirent

        # Filter only files and directorys under self.pwd
        for _, dirs, files in self.walk(self._dirent):
            for d in dirs:
                inodes_ls.setdefault(d.inode, [])
                inodes_ls[d.parent_inode].append(d)
                inodes_tree[(d.parent_inode, d.name)] = d
            for f in files:
                extents_map[f.inode] = []
                inode_map[f.inode] = None
                inodes_ls[f.parent_inode].append(f)
                inodes_tree[(f.parent_inode, f.name)] = f

        self._extents_map = extents_map
        self._inode_map = inode_map
        self._inodes_ls = inodes_ls
        self._inodes_tree = inodes_tree

        for extent in filesystem.extents():
            if extent.inode not in self._extents_map:
                continue
            self._extents_map[extent.inode].append(extent)

        for inode in filesystem.inodes():
            if (
                inode.inode not in self._inode_map
                or self._inode_map.get(inode.inode, None) is not None
            ):
                continue
            self._inode_map[inode.inode] = inode

        for inode, extents in self._extents_map.items():
            self._extents_map[inode] = self._unique_extent_list(extents)

        for parent_inode, ls in self._inodes_ls.items():
            self._inodes_ls[parent_inode] = self._unique_dirent_list(ls)

    def _walk(self, top: str, dirent: DirEnt):
        dirs = [ent for ent in self._inodes_ls[dirent.inode] if ent.is_dir]
        files = [ent for ent in self._inodes_ls[dirent.inode] if ent.is_file]
        yield top, dirs, files
        for d in dirs:
            yield from self._walk(os.path.join(top, d.name), d)

    @staticmethod
    def _unique_extent_list(inode_extents):
        # It's possible to have multiple duplicated extents for a single inode
        # and this implementation assumes that the last ones should be the
        # correct ones.
        unique_extent_list = []
        for ent in reversed(sorted(inode_extents, key=lambda _: _.file_offset)):
            if not unique_extent_list:
                unique_extent_list.append(ent)
            elif (
                ent.file_offset + ent.size == unique_extent_list[0].file_offset
            ):
                if ent.offset + ent.size == unique_extent_list[0].offset:
                    ent = Extent(
                        ent.inode,
                        ent.file_offset,
                        ent.offset,
                        ent.size + unique_extent_list[0].size,
                    )
                    unique_extent_list.pop(0)
                unique_extent_list.insert(0, ent)
        return unique_extent_list

    @staticmethod
    def _unique_dirent_list(dirent_ls):
        # It's possible to have multiple inodes for a single file and this
        # implementation assumes that the last inode should be the correct one.
        return list({ent.name: ent for ent in dirent_ls}.values())


class BcachefsIter:
    class _EmptyIter:
        def next(self):
            return None

    def __init__(self, fs: _Bcachefs, t: int = DIRENT_TYPE):
        self._iter: _Bcachefs_iterator = (
            fs.iter(t) if fs is not None else self._EmptyIter()
        )

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
        while not inode.hash_seed or inode.inode in self._deleted:
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
            or (dirent.parent_inode, dirent.name) in self._deleted
        ):
            self._deleted.add((dirent.parent_inode, dirent.name))
            dirent = DirEnt(*super(BcachefsIterDirEnt, self).__next__())
        return dirent
