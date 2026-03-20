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

/*----------------------------------------------------------------------*/
/* Python C-API Glossary (objects used in mycsv_parse_line)
 *
 * PyObject *
 *   The universal pointer type for every Python object in C.
 *   Every Python value — int, str, list, None — is a PyObject * at the C
 *   level. Functions return PyObject * so they can return any Python type
 *   (or NULL on error).
 *
 * PyArg_ParseTuple(args, "ss", &line, &delim)
 *   Unpacks a Python argument tuple into C variables.
 *   The format string "ss" means "two C strings (const char *)".
 *   Returns 0 (false) and sets a TypeError if the call doesn't match,
 *   so the canonical pattern is: if (!PyArg_ParseTuple(...)) return NULL;
 *
 *   Format string cheat-sheet:
 *     "s"  -> const char *       (UTF-8 string)
 *     "i"  -> int
 *     "l"  -> long
 *     "f"  -> float
 *     "d"  -> double
 *     "O"  -> PyObject *         (any Python object, no conversion)
 *
 *   Examples:
 *     // Python: fn("hello", ",")  ->  C: const char *s, *delim
 *     PyArg_ParseTuple(args, "ss", &s, &delim);
 *
 *     // Python: fn("hello", 42)   ->  C: const char *s; int n;
 *     PyArg_ParseTuple(args, "si", &s, &n);
 *
 *     // Python: fn(3.14)          ->  C: double d;
 *     PyArg_ParseTuple(args, "d", &d);
 *
 * PyList_New(0)
 *   Creates a new, empty Python list object (equivalent to []).
 *   Returns a PyObject * with reference count 1, or NULL on allocation
 *   failure.
 *
 * PyUnicode_FromStringAndSize(buf, len)
 *   Creates a Python str object from a C char * buffer and explicit byte
 *   length. Equivalent to Python str(buf[:len]) decoded as UTF-8.
 *   Returns NULL on failure (sets MemoryError or UnicodeDecodeError).
 *
 *   Examples:
 *     // Full string: buf = "hello" -> Python "hello"
 *     PyObject *s = PyUnicode_FromStringAndSize("hello", 5);
 *
 *     // Slice of a larger buffer: extract "foo" from "foo,bar"
 *     const char *start = "foo,bar";
 *     PyObject *s = PyUnicode_FromStringAndSize(start, 3); // -> "foo"
 *
 *     // Used with pointer arithmetic (as in this file):
 *     //   start points to field begin, p points to delimiter
 *     //   p - start gives the field length
 *     PyObject *field = PyUnicode_FromStringAndSize(start, p - start);
 *
 * PyList_Append(list, item)
 *   Appends item to list — equivalent to list.append(item) in Python.
 *   Increments item's reference count internally, so the caller must
 *   still Py_DECREF its own reference to item afterwards.
 *
 * Py_DECREF(obj)
 *   Decrements the reference count of obj. When the count hits 0 Python
 *   frees the object. Every PyObject * you "own" must eventually be
 *   decremented; forgetting it causes a memory leak.
 *----------------------------------------------------------------------*/
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
