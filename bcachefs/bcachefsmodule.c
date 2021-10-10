/* Includes */
#define  PY_SSIZE_T_CLEAN  /* So we get Py_ssize_t args. */
#include <Python.h>        /* Because of "reasons", the Python header must be first. */

#include "bcachefs.h"
#include "bcachefsmodule.h"


/* Python API Function Definitions */

/**
 * @brief Slot tp_dealloc
 */

static void PyBCacheFS_dealloc(PyBCacheFS* self)
{
    BCacheFS_fini(&self->_fs);
    Py_TYPE(self)->tp_free(self);
}

/**
 * @brief Slot tp_new
 */

static PyObject* PyBCacheFS_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    (void)args;
    (void)kwargs;
    return type->tp_alloc(type, 0);
}

/**
 * @brief
 */

static PyObject *PyBCacheFS_open(PyBCacheFS *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    (void)kwnames;
    if (nargs != 1 || !BCacheFS_open(&self->_fs, (void*)PyUnicode_1BYTE_DATA(args[0])))
    {
        PyErr_SetString(PyExc_RuntimeError, "Error opening BCacheFS image file");
        return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * @brief
 */

static PyObject *PyBCacheFS_close(PyBCacheFS *self)
{
    if (!BCacheFS_close(&self->_fs))
    {
        PyErr_SetString(PyExc_RuntimeError, "Error closing BCacheFS image file");
        return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * @brief
 */

static PyObject *PyBCacheFS_iter(PyBCacheFS *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    (void)kwnames;
    PyBCacheFS_iterator *iter = (void*)PyObject_CallObject((PyObject*)&PyBCacheFS_iteratorType, NULL);
    if (iter)
    {
        Py_INCREF(self);
        iter->_pyfs = self;
    }
    if (iter == NULL || nargs != 1 || !BCacheFS_iter(&iter->_pyfs->_fs, &iter->_iter, (enum btree_id)(int)PyLong_AsLong(args[0])))
    {
        PyErr_SetString(PyExc_RuntimeError, "Error initializing BCacheFS iterator");
        Py_XDECREF(iter);
        return NULL;
    }
    Py_INCREF(iter);
    return (PyObject*)iter;
}

/**
 * @brief Getter for length.
 */

static PyObject* PyBCacheFS_getsize(PyBCacheFS* self, void* closure)
{
    (void)closure;
    return PyLong_FromLong(self->_fs.size);
}

/**
 * Table of methods.
 */

static PyMethodDef PyBCacheFS_methods[] = {
    {"open", (PyCFunction)(_PyCFunctionFastWithKeywords)PyBCacheFS_open,
     METH_FASTCALL | METH_KEYWORDS, "Open bcachefs file to read"},
    {"close", (PyCFunction)(PyNoArgsFunction)PyBCacheFS_close, METH_NOARGS, "Close bcachefs file"},
    {"iter", (PyCFunction)(_PyCFunctionFastWithKeywords)PyBCacheFS_iter,
     METH_FASTCALL | METH_KEYWORDS, "Iterate over entries of specified type"},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

/**
 * Table of getter-setters.
 */

static PyGetSetDef PyBCacheFS_getsetters[] = {
    {"size", (getter)PyBCacheFS_getsize, 0, "Size of the image file", NULL},
    {NULL, NULL, 0, NULL, NULL}  /* Sentinel */
};

static PyTypeObject PyBCacheFSType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "benzina.c_bcachefs.BCacheFS",   /* tp_name */
    sizeof(PyBCacheFS),              /* tp_basicsize */
    0,                               /* tp_itemsize */
    (destructor)PyBCacheFS_dealloc,  /* tp_dealloc */
    0,                               /* tp_print */
    0,                               /* tp_getattr */
    0,                               /* tp_setattr */
    0,                               /* tp_reserved */
    0,                               /* tp_repr */
    0,                               /* tp_as_number */
    0,                               /* tp_as_sequence */
    0,                               /* tp_as_mapping */
    0,                               /* tp_hash  */
    0,                               /* tp_call */
    0,                               /* tp_str */
    0,                               /* tp_getattro */
    0,                               /* tp_setattro */
    0,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,              /* tp_flags */
    "BCacheFS object",               /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    PyBCacheFS_methods,              /* tp_methods */
    0,                               /* tp_members */
    PyBCacheFS_getsetters,           /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    0,                               /* tp_init */
    0,                               /* tp_alloc */
    PyBCacheFS_new,                  /* tp_new */
};

/**
 * @brief Slot tp_dealloc
 */

static void PyBCacheFS_iterator_dealloc(PyBCacheFS_iterator* self)
{
    BCacheFS_iter_fini(&self->_pyfs->_fs, &self->_iter);
    Py_XDECREF((PyObject*)self->_pyfs);
    Py_TYPE(self)->tp_free(self);
}

/**
 * @brief Slot tp_new
 */

static PyObject* PyBCacheFS_iterator_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    (void)args;
    (void)kwargs;
    return type->tp_alloc(type, 0);
}

/**
 * @brief
 */

static PyObject *PyBCacheFS_iterator_next(PyBCacheFS_iterator *self)
{
    const BCacheFS *fs = &self->_pyfs->_fs;
    BCacheFS_iterator *iter = &self->_iter;
    const struct bch_val *bch_val = BCacheFS_iter_next(fs, iter);
    if (bch_val && iter->type == BTREE_ID_extents)
    {
        BCacheFS_extent extent = BCacheFS_iter_make_extent(fs, iter);
        return Py_BuildValue("KKKK", extent.inode, extent.file_offset, extent.offset, extent.size);
    }
    else if (bch_val && iter->type == BTREE_ID_dirents)
    {
        BCacheFS_dirent dirent = BCacheFS_iter_make_dirent(fs, iter);
        return Py_BuildValue("KKIU", dirent.parent_inode, dirent.inode, (uint32_t)dirent.type, dirent.name);
    }
    else if (bch_val && iter->type == BTREE_ID_inodes)
    {
        BCacheFS_inode inode = BCacheFS_iter_make_inode(fs, iter);
        return Py_BuildValue("KK", inode.inode, inode.size);
    }
    return Py_None;
}

/**
 * Table of methods.
 */

static PyMethodDef PyBCacheFS_iterator_methods[] = {
    {"next", (PyCFunction)(PyNoArgsFunction)PyBCacheFS_iterator_next, METH_NOARGS, "Iterate to next item"},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

static PyTypeObject PyBCacheFS_iteratorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "benzina.c_bcachefs.BCacheFS_iterator",   /* tp_name */
    sizeof(PyBCacheFS_iterator),     /* tp_basicsize */
    0,                               /* tp_itemsize */
    (destructor)PyBCacheFS_iterator_dealloc,  /* tp_dealloc */
    0,                               /* tp_print */
    0,                               /* tp_getattr */
    0,                               /* tp_setattr */
    0,                               /* tp_reserved */
    0,                               /* tp_repr */
    0,                               /* tp_as_number */
    0,                               /* tp_as_sequence */
    0,                               /* tp_as_mapping */
    0,                               /* tp_hash  */
    0,                               /* tp_call */
    0,                               /* tp_str */
    0,                               /* tp_getattro */
    0,                               /* tp_setattro */
    0,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,              /* tp_flags */
    "BCacheFS_iterator object",      /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    PyBCacheFS_iterator_methods,     /* tp_methods */
    0,                               /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    0,                               /* tp_init */
    0,                               /* tp_alloc */
    PyBCacheFS_iterator_new,         /* tp_new */
};

static PyModuleDef c_bcachefs_module_def = {
    PyModuleDef_HEAD_INIT,
    "c_bcachefs",          /* m_name */
    "bcachefs C module",   /* m_doc */
    -1,                    /* m_size */
    NULL,                  /* m_methods */
    NULL,                  /* m_reload */
    NULL,                  /* m_traverse */
    NULL,                  /* m_clear */
    NULL,                  /* m_free */
};

PyMODINIT_FUNC PyInit_c_bcachefs(void)
{
    PyObject* module = NULL;

    module = PyModule_Create(&c_bcachefs_module_def);
    if (!module)
    {
        return NULL;
    }

    #define ADDTYPE(T)                                              \
        do{                                                         \
            if(PyType_Ready(&T##Type) < 0){return NULL;}            \
            Py_INCREF(&T##Type);                                    \
            PyModule_AddObject(module, #T, (PyObject*)&T##Type);    \
        }while(0)
    ADDTYPE(PyBCacheFS);
    ADDTYPE(PyBCacheFS_iterator);
    #undef ADDTYPE

    return module;
}
