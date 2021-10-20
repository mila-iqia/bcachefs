/* Include Guard */
#ifndef BCACHEFSMODULE_H
#define BCACHEFSMODULE_H

/**
 * Includes
 */

#define PY_SSIZE_T_CLEAN /* So we get Py_ssize_t args. */
#include "bcachefs.h"
#include <Python.h> /* Because of "reasons", the Python header must be first. */

/* Type Definitions and Forward Declarations */
typedef struct
{
    PyObject_HEAD;
    Bcachefs _fs;
} PyBcachefs;
static PyTypeObject PyBcachefsType;

typedef struct
{
    PyObject_HEAD;
    PyBcachefs*       _pyfs;
    Bcachefs_iterator _iter;
} PyBcachefs_iterator;
static PyTypeObject PyBcachefs_iteratorType;

#endif // BCACHEFSMODULE_H
