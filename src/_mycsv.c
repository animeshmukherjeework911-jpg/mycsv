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

/* See PYTHON_C_API.md at the project root for a full glossary of the
 * Python C API calls used in this file. */

 static PyObject *mycsv_Error = NULL;

static PyObject *mycsv_parse_line_no_quote(PyObject *self, PyObject *args, PyObject *kwargs)
{

    const char *line;
    const char *delim = ",";

    static char *kwlist[] = {"line", "delimiter", NULL};

    // if (!PyArg_ParseTuple(args, "ss", &line, &delim))
    //     return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|s", kwlist, &line, &delim))
        return NULL;

    if (strlen(delim) != 1) {
        PyErr_SetString(mycsv_Error, "delimiter must be a 1-character string");
        return NULL;
    }

    char delimiter = delim[0];

    PyObject *result = PyList_New(0);
    if (!result)
    {
        return NULL;
    }

    const char *start = line;
    const char *p = line;

    while (1)
    {
        if (*p == delimiter || *p == '\0')
        {
            PyObject *field = PyUnicode_FromStringAndSize(start, p - start);
            if (!field)
            {
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
}

static PyObject *mycsv_parse_line(PyObject *self, PyObject *args, PyObject *kwargs)
{

    const char *line;
    const char *delim = ",";
    const char *quotestr = "\"";

    static char *kwlist[] = {"line", "delimiter", "quotechar", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ss", kwlist, &line, &delim, &quotestr))
        return NULL;

    if (strlen(delim) != 1) {
        PyErr_SetString(mycsv_Error, "delimiter must be a 1-character string");
        return NULL;
    }
    if (strlen(quotestr) != 1) {
        PyErr_SetString(mycsv_Error, "quotechar must be a 1-character string");
        return NULL;
    }

    char sep = delim[0];
    char quote = quotestr[0];

    PyObject *result = PyList_New(0);
    if (!result)
    {
        return NULL;
    }

    Py_ssize_t buf_size = 128;
    char *buf = (char *)PyMem_Malloc(buf_size);
    if (!buf)
    {
        Py_DECREF(result);
        return PyErr_NoMemory();
    }
    Py_ssize_t buf_len = 0;

#define BUF_APPEND(c)                                         \
    do                                                        \
    {                                                         \
        if (buf_len >= buf_size - 1)                          \
        {                                                     \
            buf_size *= 2;                                    \
            char *tmp = (char *)PyMem_Realloc(buf, buf_size); \
            if (!tmp)                                         \
            {                                                 \
                PyMem_Free(buf);                              \
                Py_DECREF(result);                            \
                return PyErr_NoMemory();                      \
            }                                                 \
            buf = tmp;                                        \
        }                                                     \
        buf[buf_len++] = (c);                                 \
    } while (0)

    typedef enum
    {
        FIELD_START,
        IN_FIELD,
        IN_QUOTED
    } State;
    State state = FIELD_START;
    const char *p = line;

    while (1)
    {
        char c = *p;
        if (state == FIELD_START)
        {
            if (c == quote)
            {
                state = IN_QUOTED;
            }
            else if (c == sep || c == '\0')
            {
                PyObject *field = PyUnicode_FromStringAndSize(buf, 0);
                if (!field)
                    goto error;
                PyList_Append(result, field);
                Py_DECREF(field);
                if (c == '\0')
                    goto done;
                state = FIELD_START;
            }
            else
            {
                BUF_APPEND(c);
                state = IN_FIELD;
            }
        }
        else if (state == IN_FIELD)
        {
            if (c == sep || c == '\0')
            {
                PyObject *field = PyUnicode_FromStringAndSize(buf, buf_len);
                if (!field)
                    goto error;
                PyList_Append(result, field);
                Py_DECREF(field);
                buf_len = 0;
                if (c == '\0')
                    goto done;
                state = FIELD_START;
            }
            else
            {
                BUF_APPEND(c);
            }
        }
        else
        {
            if (c == '\0')
            {
                PyErr_SetString(mycsv_Error, "mycsv: unclosed quote");
                goto error;
            }
            else if (c == quote)
            {
                if (*(p + 1) == quote)
                {
                    BUF_APPEND(c);
                    p++;
                }
                else if (*(p + 1) == sep || *(p + 1) == '\0')
                {
                    PyObject *field = PyUnicode_FromStringAndSize(buf, buf_len);
                    if (!field)
                        goto error;
                    PyList_Append(result, field);
                    Py_DECREF(field);
                    buf_len = 0;
                    p++;
                    if (*p == '\0')
                        goto done;
                    state = FIELD_START;
                }
                else
                {
                    PyErr_SetString(mycsv_Error,
                                    "mycsv: unexpected char after closing quote");
                    goto error;
                }
            }
            else
            {
                BUF_APPEND(c);
            }
        }
        p++;
    }

done:
    PyMem_Free(buf);
    return result;

error:
    PyMem_Free(buf);
    Py_DECREF(result);
    return NULL;
}
#undef BUF_APPEND

static PyObject *mycsv_format_row(PyObject *self, PyObject *args, PyObject *kwargs)
{

    PyObject *fields_obj;
    const char *delim = ",";
    const char *quotestr = "\"";
    const char *lineterm = "\r\n";
    int quoting = 0;

    static char *kwlist[] = {"fields", "delimiter", "quotechar", "lineterminator", "quoting", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|sssi", kwlist, &fields_obj, &delim, &quotestr, &lineterm, &quoting))
        return NULL;

    if (strlen(delim) != 1) {
        PyErr_SetString(mycsv_Error, "delimiter must be a 1-character string");
        return NULL;
    }
    if (strlen(quotestr) != 1) {
        PyErr_SetString(mycsv_Error, "quotechar must be a 1-character string");
        return NULL;
    }
    if (quoting < 0 || quoting > 3) {
        PyErr_SetString(mycsv_Error, "quoting must be one of QUOTE_MINIMAL(0), QUOTE_ALL(1), QUOTE_NONNUMERIC(2), QUOTE_NONE(3)");
        return NULL;
    }

    char sep = delim[0];
    char quote = quotestr[0];

    PyObject *seq = PySequence_Fast(fields_obj, "format_row: fields must be iterable");

    if (!seq)
        return NULL;

    Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);

    Py_ssize_t buf_size = 256;
    char *buf = (char *)PyMem_Malloc(buf_size);
    if (!buf)
    {
        Py_DECREF(seq);
        return PyErr_NoMemory();
    }
    Py_ssize_t buf_len = 0;

#define BUF_APPEND(c)                                         \
    do                                                        \
    {                                                         \
        if (buf_len >= buf_size - 1)                          \
        {                                                     \
            buf_size *= 2;                                    \
            char *tmp = (char *)PyMem_Realloc(buf, buf_size); \
            if (!tmp)                                         \
            {                                                 \
                PyMem_Free(buf);                              \
                Py_DECREF(seq);                               \
                return PyErr_NoMemory();                      \
            }                                                 \
            buf = tmp;                                        \
        }                                                     \
        buf[buf_len++] = (c);                                 \
    } while (0)

#define BUF_APPEND_STR(s)                     \
    do                                        \
    {                                         \
        for (const char *_p = (s); *_p; _p++) \
            BUF_APPEND(*_p);                  \
    } while (0)

    for (Py_ssize_t i = 0; i < n; i++)
    {
        if (i > 0)
            BUF_APPEND(sep);
        PyObject *item = PySequence_Fast_GET_ITEM(seq, i);
        Py_ssize_t field_len;
        const char *field = PyUnicode_AsUTF8AndSize(item, &field_len);
        if (!field)
        {
            PyMem_Free(buf);
            Py_DECREF(seq);
            return NULL;
        }

        /* Decide if this field needs quoting (see PYTHON_C_API.md for quoting mode details). */
        int needs_quote = 0;

        if (quoting == 1)
        {
            needs_quote = 1;
        }
        else if (quoting == 3)
        {
            needs_quote = 0;
        }
        else
        {
            for (Py_ssize_t j = 0; j < field_len; j++)
            {
                if (field[j] == sep || field[j] == quote || field[j] == '\r' || field[j] == '\n')
                {
                    needs_quote = 1;
                    break;
                }
            }
        }

        if (needs_quote)
        {
            BUF_APPEND(quote);
            for (Py_ssize_t j = 0; j < field_len; j++)
            {
                if (field[j] == quote)
                {
                    BUF_APPEND(quote);
                }
                BUF_APPEND(field[j]);
            }
            BUF_APPEND(quote);
        }
        else
        {
            for (Py_ssize_t j = 0; j < field_len; j++)
            {
                BUF_APPEND(field[j]);
            }
        }
    }

    BUF_APPEND_STR(lineterm);

    PyObject *result = PyUnicode_FromStringAndSize(buf, buf_len);
    PyMem_Free(buf);
    Py_DECREF(seq);
    return result;
}
#undef BUF_APPEND
#undef BUF_APPEND_STR

static PyMethodDef _mycsv_methods[] = {
    {"format_row",
     (PyCFunction)mycsv_format_row,
     METH_VARARGS | METH_KEYWORDS,
     "format_row(fields, delimiter=',', quotechar='\"', lineterminator='\\r\\n') -> str\n"
     "Format a sequence of fields into a csv row string."},
    {"parse_line_no_quote",
     (PyCFunction)mycsv_parse_line_no_quote,
     METH_VARARGS | METH_KEYWORDS,
     "parse_line_no_quote(line, delimiter=',') -> list[str]\n"
     "Split a CSV line on a delimiter. No quote handling yet."},
    {"parse_line",
     (PyCFunction)mycsv_parse_line,
     METH_VARARGS | METH_KEYWORDS,
     "parse_line(line, delimiter=',', quotechar='\"') -> list[str]\n"
     "Split a CSV line with full quote and escape handling."},
    {NULL, NULL, 0, NULL}};

/* ------------------------------------------------------------------ */
/* Module init — required entry point; Python calls this on import     */
/* ------------------------------------------------------------------ */
static PyModuleDef _mycsv_module = {
    PyModuleDef_HEAD_INIT,
    "_mycsv",          /* module name */
    "mycsv C backend", /* docstring   */
    -1,                /* per-interpreter state size; -1 = none */
    _mycsv_methods     /* method table — you'll add this */
};

PyMODINIT_FUNC
PyInit__mycsv(void)
{
    PyObject *m = PyModule_Create(&_mycsv_module);
    if (!m)
        return NULL;

    /* Define the Error class directly in the C extension.
     * "_mycsv.Error" is the fully qualified name Python sees.
     * mycsv.py imports it as: from _mycsv import Error
     * This avoids the circular import that occurs when _mycsv tries to
     * import mycsv (which itself imports _mycsv) to retrieve mycsv.Error. */
    mycsv_Error = PyErr_NewException("_mycsv.Error", NULL, NULL);
    if (!mycsv_Error) {
        Py_DECREF(m);
        return NULL;
    }
    Py_INCREF(mycsv_Error);
    PyModule_AddObject(m, "Error", mycsv_Error);

    PyModule_AddIntConstant(m, "QUOTE_MINIMAL", 0);
    PyModule_AddIntConstant(m, "QUOTE_ALL", 1);
    PyModule_AddIntConstant(m, "QUOTE_NONNUMERIC", 2);
    PyModule_AddIntConstant(m, "QUOTE_NONE", 3);

    return m;
}