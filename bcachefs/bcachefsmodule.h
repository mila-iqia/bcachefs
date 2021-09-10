/* Include Guard */
#ifndef BCACHEFSMODULE_H
#define BCACHEFSMODULE_H

/**
 * Includes
 */

#define  PY_SSIZE_T_CLEAN     /* So we get Py_ssize_t args. */
#include <Python.h>           /* Because of "reasons", the Python header must be first. */
#include "bcachefs.h"

/* Type Definitions and Forward Declarations */
typedef struct {
    PyObject_HEAD
    BCacheFS _fs;
} PyBCacheFS;
static PyTypeObject PyBCacheFSType;

typedef struct {
    PyObject_HEAD
    PyBCacheFS *_pyfs;
    BCacheFS_iterator _iter;
} PyBCacheFS_iterator;
static PyTypeObject PyBCacheFS_iteratorType;

#endif // BCACHEFSMODULE_H
