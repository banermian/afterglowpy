#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_11_API_VERSION
#include <numpy/arrayobject.h>
#include "shockEvolution.h"

static char shock_docstring[] = 
    "This module calculates evolution of relativistic shocks.";
static char shockEvolRK4_docstring[] = 
    "Evolve a shock with RK4";

static PyObject *error_out(PyObject *m);
static PyObject *shock_shockEvolRK4(PyObject *self, PyObject *args);

struct module_state
{
    PyObject *error;
};
#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state *) PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

static PyMethodDef shockMethods[] = {
    {"shockEvolRK4", shock_shockEvolRK4, METH_VARARGS, shockEvolRK4_docstring},
    {"error_out", (PyCFunction)error_out, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}};

#if PY_MAJOR_VERSION >= 3

static int shock_traverse(PyObject *m, visitproc visit, void *arg)
{
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int shock_clear(PyObject *m)
{
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef shockModule = {
    PyModuleDef_HEAD_INIT,
    "shock", /* Module Name */
    shock_docstring,
    sizeof(struct module_state),
    shockMethods,
    NULL,
    shock_traverse,
    shock_clear,
    NULL
};
#define INITERROR return NULL

PyMODINIT_FUNC PyInit_shock(void)
#else
#define INITERROR return

void initshock(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&shockModule);
#else
    PyObject *module = Py_InitModule3("shock", shockMethods, shock_docstring);
#endif
    if(module == NULL)
        INITERROR;
    struct module_state *st = GETSTATE(module);
    st->error = PyErr_NewException("shock.Error", NULL, NULL);
    if(st->error == NULL)
    {
        Py_DECREF(module);
        INITERROR;
    }

    //Load numpy stuff!
    import_array();
#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}

static PyObject *error_out(PyObject *m)
{
    struct module_state *st = GETSTATE(m);
    PyErr_SetString(st->error, "something bad happened");
    return NULL;
}

static PyObject *shock_shockEvolRK4(PyObject *self, PyObject *args)
{
    PyObject *t_obj = NULL;

    double R0, u0;
    double Mej, rho0, Einj, k, umin;

    //Parse Arguments
    if(!PyArg_ParseTuple(args, "Oddddddd", &t_obj, &R0, &u0, &Mej, &rho0,
                                            &Einj, &k, &umin))
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not parse arguments.");
        return NULL;
    }

    //Grab NUMPY arrays
    PyArrayObject *t_arr;
    t_arr = (PyArrayObject *) PyArray_FROM_OTF(t_obj, NPY_DOUBLE,
                                                NPY_ARRAY_IN_ARRAY);
    if(t_arr == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not read input arrays.");
        Py_XDECREF(t_arr);
        return NULL;
    }

    int t_ndim = (int) PyArray_NDIM(t_arr);
    if(t_ndim != 1)
    {
        PyErr_SetString(PyExc_RuntimeError, "Array must be 1-D.");
        Py_DECREF(t_arr);
        return NULL;
    }

    int N = (int)PyArray_DIM(t_arr, 0);

    double *t = (double *)PyArray_DATA(t_arr);

    //Allocate output array

    npy_intp dims[1] = {N};
    PyObject *R_obj = PyArray_SimpleNew(1, dims, NPY_DOUBLE);
    PyObject *u_obj = PyArray_SimpleNew(1, dims, NPY_DOUBLE);

    if(R_obj == NULL || u_obj == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not make output arrays.");
        Py_DECREF(t_arr);
        Py_XDECREF(R_obj);
        Py_XDECREF(u_obj);
        return NULL;
    }
    double *R = PyArray_DATA((PyArrayObject *) R_obj);
    double *u = PyArray_DATA((PyArrayObject *) u_obj);

    // Evolve the shock!
    double shockArgs[6] = {u0, Mej, rho0, Einj, k, umin};
    shockEvolveRK4(t, R, u, N, R0, u0, shockArgs);

    // Clean up!
    Py_DECREF(t_arr);

    //Build output
    PyObject *ret = Py_BuildValue("NN", R_obj, u_obj);
    
    return ret;
}