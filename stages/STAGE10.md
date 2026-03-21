# Stage 10 — `mycsv.Error` + full edge-case handling

Define `mycsv.Error` as a proper custom exception type registered in the
C module. Handle all remaining edge cases to reach full stdlib compatibility.
Introduces custom exception types in C.

---

## Sub-step A — Register `mycsv.Error` in the C module

Instead of `PyExc_ValueError`, the C extension should raise `mycsv.Error`.
Since `Error` lives in `mycsv.py` (Python layer), the cleanest approach is
to store a reference to it in the C module at import time.

In `_mycsv.c`, add a module-level global:

```c
static PyObject *mycsv_Error = NULL;  /* set during module init */
```

In `PyInit__mycsv`, after `PyModule_Create`:

```c
/* Import mycsv.Error so C code can raise it directly */
PyObject *mycsv_mod = PyImport_ImportModule("mycsv");
if (mycsv_mod) {
    mycsv_Error = PyObject_GetAttrString(mycsv_mod, "Error");
    Py_DECREF(mycsv_mod);
}
/* Fall back to ValueError if mycsv isn't importable yet (e.g. during build) */
if (!mycsv_Error) {
    PyErr_Clear();
    mycsv_Error = PyExc_ValueError;
}
```

Replace all `PyErr_SetString(PyExc_ValueError, ...)` in `parse_line` with:

```c
PyErr_SetString(mycsv_Error, "mycsv: unclosed quote");
```

---

## Sub-step B — Edge cases to handle

### 1. Empty input string
```python
list(mycsv.reader([""])) == []   # blank line skipped
```

### 2. Single field, no delimiter
```python
_mycsv.parse_line("hello", ",") == ["hello"]
```

### 3. Trailing delimiter
```python
_mycsv.parse_line("a,b,", ",") == ["a", "b", ""]
```

### 4. All-whitespace field
```python
_mycsv.parse_line("a, ,b", ",") == ["a", " ", "b"]  # space preserved
```

### 5. Embedded newline inside quoted field (not supported — raise Error)
```python
# parse_line operates on a single line; multi-line quoted fields
# are the responsibility of the reader layer (future extension)
```

### 6. `writer` with non-string types
```python
buf = io.StringIO()
mycsv.writer(buf).writerow([1, 2.5, None, True])
# → '1,2.5,,True\r\n'   (None → "")
```

Update `writer.writerow` to handle `None`:
```python
fields = [("" if f is None else str(f)) for f in row]
```

---

## Sub-step C — Final compatibility test suite

```python
import csv, mycsv, io

EDGE_CASES = [
    # (description, rows)
    ("empty fields",       [["", "", ""]]),
    ("quoted delimiter",   [["a,b", "c"]]),
    ("doubled quote",      [['say "hi"', "x"]]),
    ("trailing delimiter", [["a", "b", ""]]),
    ("numeric coercion",   [[1, 2.5, None]]),
    ("tab delimiter",      [["a\tb", "c"]]),
]

for desc, rows in EDGE_CASES:
    buf = io.StringIO()
    mycsv.writer(buf).writerows(rows)
    buf.seek(0)
    result = list(mycsv.reader(buf))
    # Coerce expected to strings for comparison
    expected = [[('' if f is None else str(f)) for f in row] for row in rows]
    assert result == expected, f"FAIL [{desc}]: {result!r} != {expected!r}"
    print(f"OK  [{desc}]")
```

---

## Sub-step D — Run the full test suite

```bash
pytest tests/ -v
```

All tests across all steps should pass.

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| `PyImport_ImportModule` | Importing a Python module from C |
| `PyObject_GetAttrString` | Getting an attribute (e.g. `Error`) from a module |
| Custom exception in C | Raising `mycsv.Error` instead of `ValueError` |
| `None` handling in writer | Coercing Python `None` to empty string |
| Edge-case parity with stdlib | Ensuring behaviour matches `csv` module |

---

## Final Checklist — Project Complete

- [ ] `mycsv.Error` is raised (not `ValueError`) on malformed CSV
- [ ] All edge cases in Sub-step B pass
- [ ] `writer.writerow([1, None, 2.5])` → `'1,,2.5\r\n'`
- [ ] Cross-compatibility test with stdlib `csv` passes
- [ ] `pytest tests/ -v` — all tests green
- [ ] `python3 -c "import mycsv; help(mycsv)"` shows readable docstrings

---

## Project Summary

You have built a working drop-in replacement for Python's `csv` module:

| Layer | File | Responsibility |
|---|---|---|
| C extension | `src/_mycsv.c` | Fast parsing (`parse_line`) and formatting (`format_row`) |
| Python API | `src/mycsv.py` | `reader`, `writer`, `DictReader`, `DictWriter`, dialects, `Error` |

C API concepts covered across all stages:

`PyArg_ParseTuple` · `PyArg_ParseTupleAndKeywords` · `PyList_New` ·
`PyList_Append` · `PyUnicode_FromStringAndSize` · `PyUnicode_AsUTF8AndSize` ·
`PyUnicode_Concat` · `PySequence_Fast` · `PyErr_SetString` · `PyErr_NoMemory` ·
`PyMem_Malloc` · `PyMem_Free` · `PyModule_AddIntConstant` ·
`PyImport_ImportModule` · `PyObject_GetAttrString` · `Py_DECREF` · `Py_INCREF`
