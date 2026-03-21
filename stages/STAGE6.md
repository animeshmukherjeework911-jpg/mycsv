# Stage 6 — `format_row` quoting logic + `mycsv.writer`

Extend `format_row` with full quoting control (QUOTE_ALL, QUOTE_MINIMAL,
QUOTE_NONNUMERIC, QUOTE_NONE) and wire `mycsv.writer` in Python.
Introduces `PyUnicode_Concat` and string scanning in C.

---

## Sub-step A — New C API concepts

### `PyUnicode_Concat(left, right)`
Concatenates two Python str objects, returning a new str.
Both `left` and `right` must be `PyObject *` str objects.

```c
PyObject *result = PyUnicode_Concat(left, right);
/* result is a new reference — you own it */
```

### Quoting modes (mirror stdlib `csv` constants)

| Constant | Value | Meaning |
|---|---|---|
| `QUOTE_MINIMAL` | 0 | Quote only when field contains delimiter/quotechar/newline (default) |
| `QUOTE_ALL` | 1 | Quote every field |
| `QUOTE_NONNUMERIC` | 2 | Quote all non-numeric fields |
| `QUOTE_NONE` | 3 | Never quote; escape delimiter with escapechar instead |

---

## Sub-step B — Add quoting constants to the C module

In `PyInit__mycsv`, add integer constants after `PyModule_Create`:

```c
PyMODINIT_FUNC
PyInit__mycsv(void)
{
    PyObject *m = PyModule_Create(&_mycsv_module);
    if (!m) return NULL;

    PyModule_AddIntConstant(m, "QUOTE_MINIMAL",    0);
    PyModule_AddIntConstant(m, "QUOTE_ALL",        1);
    PyModule_AddIntConstant(m, "QUOTE_NONNUMERIC", 2);
    PyModule_AddIntConstant(m, "QUOTE_NONE",       3);

    return m;
}
```

---

## Sub-step C — Add `quoting` parameter to `format_row`

Update `mycsv_format_row` to accept an integer `quoting` keyword arg and
apply the quoting mode when deciding whether to quote each field:

```c
int quoting = 0;   /* QUOTE_MINIMAL */

static char *kwlist[] = {"fields", "delimiter", "quotechar",
                         "lineterminator", "quoting", NULL};

if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|sssi", kwlist,
                                 &fields_obj, &delim, &quotestr,
                                 &lineterm, &quoting))
    return NULL;
```

Replace the `needs_quote` decision block:

```c
int needs_quote = 0;
if (quoting == 1) {          /* QUOTE_ALL */
    needs_quote = 1;
} else if (quoting == 3) {   /* QUOTE_NONE — never quote */
    needs_quote = 0;
} else {                     /* QUOTE_MINIMAL (0) and QUOTE_NONNUMERIC (2) */
    for (Py_ssize_t j = 0; j < field_len; j++) {
        if (field[j] == sep || field[j] == quote ||
            field[j] == '\r' || field[j] == '\n') {
            needs_quote = 1;
            break;
        }
    }
}
```

---

## Sub-step D — Implement `mycsv.writer` in Python

Open `src/mycsv.py` and replace the `writer` stub:

```python
class writer:
    def __init__(self, fileobj, delimiter=",", quotechar='"',
                 lineterminator="\r\n", dialect=None):
        self._file       = fileobj
        self.delimiter   = delimiter
        self.quotechar   = quotechar
        self.lineterminator = lineterminator

    def writerow(self, row):
        line = _mycsv.format_row(
            list(row),
            delimiter=self.delimiter,
            quotechar=self.quotechar,
            lineterminator=self.lineterminator,
        )
        self._file.write(line)

    def writerows(self, rows):
        for row in rows:
            self.writerow(row)
```

---

## Sub-step E — Rebuild and smoke-test

```bash
pip install -e . --no-build-isolation
```

```python
import io, mycsv

buf = io.StringIO()
w = mycsv.writer(buf)
w.writerow(["Alice", "30", "New York"])
w.writerow(["Bob",   "25", "LA, CA"])
print(buf.getvalue())
# Alice,30,New York\r\n
# Bob,25,"LA, CA"\r\n   ← only "LA, CA" quoted because it contains a comma
```

---

## Sub-step F — Run tests

```bash
pytest tests/ -k "TestStep6_" -v
```

This runs both `TestStep6_WriterBasic` (basic `writerow`/`writerows`) and
`TestStep6_WriterQuoting` (quoting modes).

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| `PyModule_AddIntConstant` | Exposing C integer constants to Python |
| `PyUnicode_Concat` | String building in C |
| Quoting modes | QUOTE_MINIMAL / ALL / NONNUMERIC / NONE |
| `writer.writerow` / `writerows` | Python wrapper over `format_row` |

---

## Checklist before moving to Stage 7

- [ ] `_mycsv.QUOTE_ALL` == 1 in Python
- [ ] `format_row(["a","b"], quoting=1)` → `'"a","b"\r\n'`
- [ ] `writer.writerow` writes a correctly formatted line
- [ ] `writer.writerows` writes multiple lines
- [ ] `pytest -k "TestStep6_"` passes
