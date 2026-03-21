# Stage 3 — Delimiter keyword argument + `mycsv.reader`

Complete Step 3 (keyword args in C) then wire `mycsv.reader` in Python.
Run tests after each sub-step.

---

## Sub-step A — Add keyword argument support to `parse_line`

Right now `parse_line` only accepts positional args via `PyArg_ParseTuple`.
Step 3 upgrades it to also accept `delimiter` as a keyword argument using
`PyArg_ParseTupleAndKeywords`.

Open `src/_mycsv.c` and replace the `mycsv_parse_line` implementation:

```c
static PyObject *
mycsv_parse_line(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *line;
    const char *delim = ",";   /* default delimiter */

    static char *kwlist[] = {"line", "delimiter", NULL};

    /* "s|s" means: one required string, one optional string.
     * PyArg_ParseTupleAndKeywords handles both positional and keyword args. */
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|s", kwlist,
                                     &line, &delim))
        return NULL;

    char sep = delim[0];

    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    const char *start = line;
    const char *p     = line;

    while (1) {
        if (*p == sep || *p == '\0') {
            PyObject *field = PyUnicode_FromStringAndSize(start, p - start);
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
}
```

Update the method table entry to use `METH_VARARGS | METH_KEYWORDS`:

```c
static PyMethodDef _mycsv_methods[] = {
    {"parse_line",
     (PyCFunction)mycsv_parse_line,
     METH_VARARGS | METH_KEYWORDS,
     "parse_line(line, delimiter=',') -> list[str]\n"
     "Split a CSV line on delimiter. No quote handling yet."},
    {NULL, NULL, 0, NULL}
};
```

**Rebuild:**
```bash
pip install -e . --no-build-isolation
```

**Smoke-test:**
```python
import _mycsv
print(_mycsv.parse_line("a,b,c"))           # → ['a', 'b', 'c']  (default delimiter)
print(_mycsv.parse_line("a|b|c", "|"))      # → ['a', 'b', 'c']  (positional)
print(_mycsv.parse_line("a|b|c", delimiter="|"))  # → ['a', 'b', 'c']  (keyword)
```

---

## Sub-step B — Implement `mycsv.reader` in Python

Open `src/mycsv.py` and replace the `reader` stub:

```python
import _mycsv

class Error(Exception):
    pass

class reader:
    def __init__(self, iterable, delimiter=",", quotechar='"', dialect=None):
        self._lines = iter(iterable)
        self.delimiter = delimiter
        self.quotechar = quotechar
        self.line_num = 0

    def __iter__(self):
        return self

    def __next__(self):
        line = next(self._lines)          # raises StopIteration when done
        self.line_num += 1
        # Strip the trailing newline that file objects include
        line = line.rstrip("\r\n")
        return _mycsv.parse_line(line, self.delimiter)

def writer(fileobj, delimiter=",", quotechar='"', lineterminator="\r\n", dialect=None):
    raise NotImplementedError
```

---

## Sub-step C — Run the Step 3 tests

```bash
pytest tests/ -k "TestStep1_ or TestStep2_ or TestStep3_" -v
```

Expected: Step 1 + Step 2 pass. Step 3 reader tests pass for basic cases
(quoted field tests will fail — those come in Stage 4).

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| `PyArg_ParseTupleAndKeywords` | Keyword argument support in C |
| `METH_VARARGS \| METH_KEYWORDS` | Telling Python the function accepts kwargs |
| `static char *kwlist[]` | Declaring the keyword argument name list |
| `"s\|s"` format string | Optional argument with `\|` in format |
| `reader.__iter__` + `__next__` | Making a Python class an iterator |

---

## Checklist before moving to Stage 4

- [ ] `_mycsv.parse_line("a,b")` works (default delimiter)
- [ ] `_mycsv.parse_line("a|b", delimiter="|")` works (keyword arg)
- [ ] `list(mycsv.reader(["a,b\n", "c,d\n"]))` returns `[['a','b'], ['c','d']]`
- [ ] `pytest -k "TestStep1_ or TestStep2_ or TestStep3_"` passes
