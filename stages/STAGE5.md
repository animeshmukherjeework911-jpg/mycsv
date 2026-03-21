# Stage 5 — `_mycsv.format_row()` basic implementation

Add `format_row` to the C extension. Introduces `PySequence_Fast` for
iterating over any Python sequence from C, and building strings in C.

---

## Sub-step A — New C API concepts

### `PySequence_Fast(obj, msg)`
Converts any Python iterable (list, tuple, generator) into a list or tuple
that supports O(1) indexed access. Returns a new reference, or NULL on error.

```c
PyObject *seq = PySequence_Fast(fields, "format_row: expected iterable");
if (!seq) return NULL;
Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
PyObject *item = PySequence_Fast_GET_ITEM(seq, i);  /* borrowed ref — do NOT DECREF */
```

### `PyUnicode_AsUTF8AndSize(obj, &size)`
Returns a `const char *` (UTF-8 bytes) for a Python str, and sets `size`.
The pointer is owned by the str object — do NOT free it.

### `PyUnicode_FromString(str)`
Creates a Python str from a NUL-terminated C string.

---

## Sub-step B — Implement `format_row` in `_mycsv.c`

Add this function above the method table:

```c
static PyObject *
mycsv_format_row(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *fields_obj;
    const char *delim      = ",";
    const char *quotestr   = "\"";
    const char *lineterm   = "\r\n";

    static char *kwlist[] = {"fields", "delimiter", "quotechar",
                             "lineterminator", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|sss", kwlist,
                                     &fields_obj, &delim, &quotestr, &lineterm))
        return NULL;

    char sep   = delim[0];
    char quote = quotestr[0];

    PyObject *seq = PySequence_Fast(fields_obj,
                                    "format_row: fields must be iterable");
    if (!seq) return NULL;

    Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);

    /* Build output into a dynamic buffer. */
    Py_ssize_t buf_size = 256;
    char *buf = (char *)PyMem_Malloc(buf_size);
    if (!buf) { Py_DECREF(seq); return PyErr_NoMemory(); }
    Py_ssize_t buf_len = 0;

#define BUF_APPEND(c)                                                    \
    do {                                                                  \
        if (buf_len >= buf_size - 1) {                                   \
            buf_size *= 2;                                                \
            char *tmp = (char *)PyMem_Realloc(buf, buf_size);            \
            if (!tmp) { PyMem_Free(buf); Py_DECREF(seq);                 \
                        return PyErr_NoMemory(); }                        \
            buf = tmp;                                                    \
        }                                                                 \
        buf[buf_len++] = (c);                                            \
    } while (0)

#define BUF_APPEND_STR(s)                                                \
    do {                                                                  \
        for (const char *_p = (s); *_p; _p++) BUF_APPEND(*_p);          \
    } while (0)

    for (Py_ssize_t i = 0; i < n; i++) {
        if (i > 0) BUF_APPEND(sep);

        PyObject *item = PySequence_Fast_GET_ITEM(seq, i);  /* borrowed */
        Py_ssize_t field_len;
        const char *field = PyUnicode_AsUTF8AndSize(item, &field_len);
        if (!field) { PyMem_Free(buf); Py_DECREF(seq); return NULL; }

        /* Decide if quoting is needed. */
        int needs_quote = 0;
        for (Py_ssize_t j = 0; j < field_len; j++) {
            if (field[j] == sep || field[j] == quote ||
                field[j] == '\r' || field[j] == '\n') {
                needs_quote = 1;
                break;
            }
        }

        if (needs_quote) {
            BUF_APPEND(quote);
            for (Py_ssize_t j = 0; j < field_len; j++) {
                if (field[j] == quote) BUF_APPEND(quote);  /* double it */
                BUF_APPEND(field[j]);
            }
            BUF_APPEND(quote);
        } else {
            for (Py_ssize_t j = 0; j < field_len; j++)
                BUF_APPEND(field[j]);
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
```

Register in the method table:

```c
{"format_row",
 (PyCFunction)mycsv_format_row,
 METH_VARARGS | METH_KEYWORDS,
 "format_row(fields, delimiter=',', quotechar='\"', lineterminator='\\r\\n') -> str\n"
 "Format a sequence of fields into a CSV row string."},
```

---

## Sub-step C — Rebuild and smoke-test

```bash
pip install -e . --no-build-isolation
```

```python
import _mycsv
print(repr(_mycsv.format_row(["Alice", "30", "NYC"])))
# → 'Alice,30,NYC\r\n'

print(repr(_mycsv.format_row(["hello, world", "ok"])))
# → '"hello, world",ok\r\n'

print(repr(_mycsv.format_row(['say "hi"', "x"])))
# → '"say ""hi""",x\r\n'
```

---

## Sub-step D — Smoke-test only

No dedicated test class for Stage 5 — `format_row` is a C-level function.
The Python `writer` class that wraps it is built in Stage 6, where
`TestStep6_WriterBasic` and `TestStep6_WriterQuoting` exercise it end-to-end.

Verify manually in the REPL:

```python
import _mycsv
assert _mycsv.format_row(["a", "b"])          == 'a,b\r\n'
assert _mycsv.format_row(["a,b", "c"])        == '"a,b",c\r\n'
assert _mycsv.format_row(['a"b', "c"])        == '"a""b",c\r\n'
assert _mycsv.format_row(["line1\nline2"])    == '"line1\nline2"\r\n'
```

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| `PySequence_Fast` | Accept any iterable, get indexed access |
| `PySequence_Fast_GET_SIZE` / `GET_ITEM` | O(1) access, borrowed references |
| `PyUnicode_AsUTF8AndSize` | Read Python str bytes in C |
| Quote-when-needed logic | Fields with delimiter/quotechar/newline |
| Doubling quotechar on output | `"` → `""` inside quoted fields |

---

## Checklist before moving to Stage 6

- [ ] `format_row(["a", "b"])` → `'a,b\r\n'`
- [ ] `format_row(["a,b", "c"])` → `'"a,b",c\r\n'`
- [ ] `format_row(['a"b', "c"])` → `'"a""b",c\r\n'`
- [ ] `format_row(["line1\nline2"])` → `'"line1\nline2"\r\n'`
- [ ] All four REPL assertions above pass (no pytest class for this stage)
