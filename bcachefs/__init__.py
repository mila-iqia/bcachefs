# This Python file uses the following encoding: utf-8

__descr__ = 'Fast Dataset Archive for HPC'
__version__ = '0.1.5'
__license__ = 'BSD 3-Clause License'
__author__ = u'Satya Ortiz-Gagné'
__author_email__ = ' '
__copyright__ = u'2021 Mila'
__url__ = 'https://github.com/mila-iqia/bcachefs'


from .bcachefs import BCacheFS, Cursor, DIR_TYPE, FILE_TYPE

__all__ = [
    'BCacheFS', 
    'Cursor',
    'DIR_TYPE',
    'FILE_TYPE'
]
