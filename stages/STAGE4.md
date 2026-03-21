# Stage 4 — Quoted field handling in `parse_line`

Add a state machine to `_mycsv.c` that correctly handles quoted fields,
embedded delimiters, and doubled-quote escaping. Introduces `PyErr_SetString`.

---

## Sub-step A — State machine design

The parser needs three states:

```
FIELD_START  → beginning of a new field
IN_FIELD     → inside an unquoted field
IN_QUOTED    → inside a quoted field (started with quotechar)
```

Transitions:
- `FIELD_START` + `quotechar` → `IN_QUOTED`
- `FIELD_START` + other char  → `IN_FIELD`
- `IN_QUOTED`  + `quotechar` + `quotechar` (doubled) → stay `IN_QUOTED`, emit one `quotechar`
- `IN_QUOTED`  + `quotechar` + delimiter/`\0` → close field, → `FIELD_START`
- `IN_QUOTED`  + `quotechar` + anything else → malformed → raise `mycsv.Error`
- `IN_QUOTED`  + `\0` → unclosed quote → raise `mycsv.Error`
- `IN_FIELD`   + delimiter/`\0` → emit field, → `FIELD_START`

---

## Sub-step B — Upgrade `parse_line` in `_mycsv.c`

Update the signature to accept `quotechar` as an optional third keyword arg:

```c
static PyObject *
mycsv_parse_line(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *line;
    const char *delim   = ",";
    const char *quotestr = "\"";

    static char *kwlist[] = {"line", "delimiter", "quotechar", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ss", kwlist,
                                     &line, &delim, &quotestr))
        return NULL;

    char sep   = delim[0];
    char quote = quotestr[0];

    PyObject *result = PyList_New(0);
    if (!result)
        return NULL;

    /* Use a dynamic buffer for the current field. */
    Py_ssize_t buf_size = 128;
    char *buf = (char *)PyMem_Malloc(buf_size);
    if (!buf) { Py_DECREF(result); return PyErr_NoMemory(); }
    Py_ssize_t buf_len = 0;

    /* Helper macro: append one char to buf, growing if needed. */
#define BUF_APPEND(c)                                                   \
    do {                                                                 \
        if (buf_len >= buf_size - 1) {                                  \
            buf_size *= 2;                                               \
            char *tmp = (char *)PyMem_Realloc(buf, buf_size);           \
            if (!tmp) { PyMem_Free(buf); Py_DECREF(result);             \
                        return PyErr_NoMemory(); }                       \
            buf = tmp;                                                   \
        }                                                                \
        buf[buf_len++] = (c);                                            \
    } while (0)

    typedef enum { FIELD_START, IN_FIELD, IN_QUOTED } State;
    State state = FIELD_START;
    const char *p = line;

    while (1) {
        char c = *p;

        if (state == FIELD_START) {
            if (c == quote) {
                state = IN_QUOTED;
            } else if (c == sep || c == '\0') {
                /* empty field */
                PyObject *field = PyUnicode_FromStringAndSize(buf, 0);
                if (!field) goto error;
                PyList_Append(result, field); Py_DECREF(field);
                if (c == '\0') goto done;
                state = FIELD_START;
            } else {
                BUF_APPEND(c);
                state = IN_FIELD;
            }
        } else if (state == IN_FIELD) {
            if (c == sep || c == '\0') {
                PyObject *field = PyUnicode_FromStringAndSize(buf, buf_len);
                if (!field) goto error;
                PyList_Append(result, field); Py_DECREF(field);
                buf_len = 0;
                if (c == '\0') goto done;
                state = FIELD_START;
            } else {
                BUF_APPEND(c);
            }
        } else { /* IN_QUOTED */
            if (c == '\0') {
                PyErr_SetString(PyExc_ValueError, "mycsv: unclosed quote");
                goto error;
            } else if (c == quote) {
                if (*(p+1) == quote) {
                    BUF_APPEND(quote);   /* doubled quote → one literal quote */
                    p++;
                } else if (*(p+1) == sep || *(p+1) == '\0') {
                    /* closing quote */
                    PyObject *field = PyUnicode_FromStringAndSize(buf, buf_len);
                    if (!field) goto error;
                    PyList_Append(result, field); Py_DECREF(field);
                    buf_len = 0;
                    p++;   /* skip past the closing quote */
                    if (*p == '\0') goto done;
                    state = FIELD_START;
                } else {
                    PyErr_SetString(PyExc_ValueError,
                                    "mycsv: unexpected char after closing quote");
                    goto error;
                }
            } else {
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
```

---

## Sub-step C — Update `mycsv.reader` to pass `quotechar`

```python
def __next__(self):
    line = next(self._lines)
    self.line_num += 1
    line = line.rstrip("\r\n")
    return _mycsv.parse_line(line, self.delimiter, self.quotechar)
```

---

## Sub-step D — Rebuild and smoke-test

```bash
pip install -e . --no-build-isolation
```

```python
import _mycsv
print(_mycsv.parse_line('"hello, world",b', ","))   # → ['hello, world', 'b']
print(_mycsv.parse_line('"say ""hi""",b', ","))     # → ['say "hi"', 'b']
print(_mycsv.parse_line('"unclosed', ","))          # → ValueError: unclosed quote
```

---

## Sub-step E — Run tests

```bash
pytest tests/ -k "TestStep1_ or TestStep2_ or TestStep3_ or TestStep4_" -v
```

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| State machine in C | Three-state parser for quoted fields |
| `PyErr_SetString(exc, msg)` | Raising a Python exception from C |
| `PyErr_NoMemory()` | Shorthand for raising `MemoryError` |
| `PyMem_Malloc` / `PyMem_Free` | CPython-aware memory allocation |
| `goto error` pattern | Clean error-path in C without leaking |

---

## Checklist before moving to Stage 5

- [ ] `parse_line('"a,b",c', ",")` → `['a,b', 'c']`
- [ ] `parse_line('"say ""hi""",x', ",")` → `['say "hi"', 'x']`
- [ ] `parse_line('"unclosed', ",")` raises `ValueError`
- [ ] `pytest -k "TestStep4_"` passes

> **Out of scope for Stage 4:** multi-line fields (`\n` inside a quoted field) require
> the `reader` to stitch lines together before calling `parse_line`. That is implemented
> in Stage 5 — see `TestStep5_MultilineFields`.
