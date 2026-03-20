/*
 * _mycsv.c — C extension skeleton
 * You will fill this in during the project steps.
 *
 * Key concepts you'll implement here:
 *   - PyArg_ParseTuple / Py_BuildValue  (argument parsing & return values)
 *   - PyUnicode_* / PyList_*            (working with Python objects in C)
 *   - PyErr_SetString                   (raising Python exceptions from C)
 *   - Module init boilerplate           (PyModuleDef + PyMODINIT_FUNC)
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>

/*----------------------------------------------------------------------*/
/* Method init - required to declare methods */

static PyObject *mycsv_parse_line(PyObject *self, PyObject *args){

    const char *line;

    const char *delim;

    if (!PyArg_ParseTuple(args, "ss", &line, &delim)) 
        return NULL;

    char sep = delim[0];

    PyObject *result = PyList_New(0);
    if (!result) {
        return NULL;
    }

    const char *start = line;
    const char *p = line;

    while (1) {
        if (*p == sep || *p == '\0') {
            PyObject *field = PyUnicode_FromStringAndSize(start, p-start);
            if (!field) {
                Py_DECREF(result);
                return NULL;
            }

            PyList_Append(result, field);
            Py_DECREF(field);

            if (*p == '\0') 
                break;

            start = p + 1;
        }
        p++;
    }
    return result;
};

static PyMethodDef _mycsv_methods[] = {
    {"parse_line",
     mycsv_parse_line,
     METH_VARARGS,
     "parse_line(line, delimiter) -> list[str]\n"
     "Split a CSV line on a delimiter. No quote handling yet."},
    {NULL, NULL, 0, NULL}};

/* ------------------------------------------------------------------ */
/* Module init — required entry point; Python calls this on import     */
/* ------------------------------------------------------------------ */
static PyModuleDef _mycsv_module = {
    PyModuleDef_HEAD_INIT,
    "_mycsv",          /* module name */
    "mycsv C backend", /* docstring   */
    -1,                /* per-interpreter state size; -1 = none */
    _mycsv_methods               /* method table — you'll add this */
};

PyMODINIT_FUNC
PyInit__mycsv(void)
{
    return PyModule_Create(&_mycsv_module);
}
