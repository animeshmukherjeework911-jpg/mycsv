# Stage 2 — `parse_line()` + Step 1 stubs

Complete Step 1 (mycsv.py stubs) then Step 2 (parse_line in C).
Run tests after each sub-step to verify before moving on.

---

## Sub-step A — Fix Step 1: add stubs to `mycsv.py`

The Step 1 tests only check that `mycsv.reader`, `mycsv.writer`, and `mycsv.Error`
**exist** on the module. They don't call them yet. Add bare-minimum stubs.

Open `src/mycsv.py` and replace its contents with:

```python
import _mycsv

class Error(Exception):
    pass

def reader(iterable, delimiter=",", quotechar='"', dialect=None):
    raise NotImplementedError

def writer(fileobj, delimiter=",", quotechar='"', lineterminator="\r\n", dialect=None):
    raise NotImplementedError
```

**Verify:**
```bash
pytest tests/ -k "TestStep1_" -v
```
Expected: 3 tests pass.

---

## Sub-step B — Add the method table to `_mycsv.c`

Open `src/_mycsv.c`.

**1. Add the function signature above `_mycsv_module`:**

```c
/* Forward declaration — implementation comes next */
static PyObject *mycsv_parse_line(PyObject *self, PyObject *args);
```

**2. Add the method table between the forward declaration and `_mycsv_module`:**

```c
static PyMethodDef _mycsv_methods[] = {
    {
        "parse_line",            /* name Python sees:  _mycsv.parse_line()  */
        mycsv_parse_line,        /* C function pointer                       */
        METH_VARARGS,            /* calling convention: positional args only */
        "parse_line(line, delimiter) -> list[str]\n"
        "Split a CSV line on delimiter. No quote handling yet."
    },
    {NULL, NULL, 0, NULL}        /* sentinel — marks end of table            */
};
```

**3. Wire the method table into `_mycsv_module` — change `NULL` to `_mycsv_methods`:**

```c
static PyModuleDef _mycsv_module = {
    PyModuleDef_HEAD_INIT,
    "_mycsv",
    "mycsv C backend",
    -1,
    _mycsv_methods      /* <-- was NULL */
};
```

---

## Sub-step C — Implement `mycsv_parse_line`

Add this function **above** the method table you just wrote.

```c
static PyObject *
mycsv_parse_line(PyObject *self, PyObject *args)
{
    const char *line;      /* the CSV line to split              */
    const char *delim;     /* delimiter string, e.g. ","         */

    /* Parse the two positional string arguments.
     * "ss" means: two UTF-8 strings → const char*.
     * Returns 0 (false) on failure — Python has already set the exception. */
    if (!PyArg_ParseTuple(args, "ss", &line, &delim))
        return NULL;

    char sep = delim[0];   /* we only use the first character    */

    /* Create an empty Python list to collect fields into. */
    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;       /* OOM — PyList_New sets MemoryError  */

    const char *start = line;   /* start of the current field    */
    const char *p     = line;   /* scanning pointer              */

    while (1) {
        if (*p == sep || *p == '\0') {
            /* We've hit a delimiter or end-of-string.
             * Build a Python str from the slice [start, p). */
            PyObject *field = PyUnicode_FromStringAndSize(start, p - start);
            if (!field) {
                Py_DECREF(result);   /* clean up before returning NULL */
                return NULL;
            }

            /* Append transfers ownership to the list — list refcount goes to 2.
             * We must DECREF our own reference (bring it back to 1). */
            PyList_Append(result, field);
            Py_DECREF(field);

            if (*p == '\0')
                break;              /* end of string — we're done        */

            start = p + 1;         /* next field starts after delimiter  */
        }
        p++;
    }

    return result;   /* caller (Python) owns this reference now */
}
```

---

## Sub-step D — Rebuild and smoke-test in Python

```bash
pip install -e . --no-build-isolation
```

Then in a Python shell:
```python
import _mycsv
print(_mycsv.parse_line("Alice,30,NYC", ","))
# → ['Alice', '30', 'NYC']

print(_mycsv.parse_line("a|b|c", "|"))
# → ['a', 'b', 'c']

print(_mycsv.parse_line("solo", ","))
# → ['solo']

print(_mycsv.parse_line("", ","))
# → ['']   (one empty field — same behaviour as stdlib csv)
```

---

## Sub-step E — Run the Step 2 tests

```bash
pytest tests/ -k "TestStep1_ or TestStep2_" -v
```

Expected output:
```
tests/test_mycsv.py::TestStep1_ModuleStructure::test_reader_exists    PASSED
tests/test_mycsv.py::TestStep1_ModuleStructure::test_writer_exists    PASSED
tests/test_mycsv.py::TestStep1_ModuleStructure::test_error_class_exists PASSED
tests/test_mycsv.py::TestStep2_ReaderBasic::test_reads_header_row     FAILED  ← expected, reader not wired yet
tests/test_mycsv.py::TestStep2_ReaderBasic::test_reads_data_rows      FAILED  ← expected
tests/test_mycsv.py::TestStep2_ReaderBasic::test_reader_is_iterator   FAILED  ← expected
```

Step 2 tests fail because `mycsv.reader` still raises `NotImplementedError` —
that gets wired up in Stage 3 when you implement the reader class in `mycsv.py`.
The goal right now is Step 1 passes and `parse_line` is callable from Python.

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| `PyArg_ParseTuple(args, "ss", ...)` | Unpack two Python string args into C `const char*` |
| `PyList_New(0)` | Create an empty Python list from C |
| `PyUnicode_FromStringAndSize(ptr, len)` | Create a Python str from a C char slice |
| `PyList_Append(list, item)` | Append a Python object to a list |
| `Py_DECREF(obj)` | Release your reference after appending |
| `PyMethodDef[]` + sentinel | Register C functions into the module |
| `return NULL` on error | How C signals exception to Python |

---

## Checklist before moving to Stage 3

- [ ] `pytest -k "TestStep1_"` → 3 pass
- [ ] `_mycsv.parse_line("a,b,c", ",")` returns `['a', 'b', 'c']` in Python shell
- [ ] `_mycsv.parse_line("a|b", "|")` works with a different delimiter
- [ ] `pip install -e .` rebuilds without warnings
