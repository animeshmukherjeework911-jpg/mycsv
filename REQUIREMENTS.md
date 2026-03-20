# mycsv — Project Requirements

## Project Goal

Build a drop-in replacement for Python's stdlib `csv` module. The parsing
hot-path (field splitting, quote handling, escape handling) lives in a C
extension (`_mycsv.c`) for performance. A Python wrapper (`mycsv.py`) provides
the ergonomic public API (reader, writer, DictReader, DictWriter, dialects).

The goal is not to beat `csv` in benchmarks — it's to understand:
- How CPython's C API works at the object level
- How Python and C exchange data across the boundary
- How the stdlib itself is structured (many stdlib modules use the same pattern)

---

## Architecture: How the Two Layers Connect

```
Your Python code
      │
      ▼
  mycsv.py          ← public API: reader(), writer(), DictReader, DictWriter
      │  imports
      ▼
  _mycsv.so         ← compiled from _mycsv.c via setup.py / pip install -e .
      │  calls
      ▼
  CPython C API     ← PyUnicode_*, PyList_*, PyArg_ParseTuple, etc.
```

Key insight: `_mycsv.so` is just a shared library. Python's import system finds
it the same way it finds `.py` files. The only difference is that its symbols
are C functions, not Python bytecode.

The C extension exposes raw, fast primitives:
- `_mycsv.parse_line(line, delimiter, quotechar)` → list of str
- `_mycsv.format_row(fields, delimiter, quotechar, lineterminator)` → str

`mycsv.py` wraps these into stateful iterator objects and higher-level classes.

---

## 10-Step Build Plan

| Step | What you build                          | New C API concepts                          |
|------|-----------------------------------------|---------------------------------------------|
| 1    | Module skeleton + can be imported       | `PyModuleDef`, `PyMODINIT_FUNC`             |
| 2    | `_mycsv.parse_line()` — no quotes       | `PyArg_ParseTuple`, `PyList_*`, `PyUnicode_*` |
| 3    | Delimiter parameter                     | keyword args from C, `PyArg_ParseTupleAndKeywords` |
| 4    | Quoted field handling in `parse_line`   | State machine in C, `PyErr_SetString`       |
| 5    | `_mycsv.format_row()` basic             | `PySequence_Fast`, building strings in C    |
| 6    | `format_row` quoting logic              | String scanning, `PyUnicode_Concat`         |
| 7    | `mycsv.reader` + `mycsv.writer` in Python | Python wrapping C primitives, `__iter__`  |
| 8    | `DictReader` + `DictWriter`             | Pure Python layer on top of reader/writer   |
| 9    | Dialect registry                        | Python class + dict registry pattern        |
| 10   | Error class + full edge-case handling   | Custom exception type in C module           |

---

## Functional Requirements

### reader
- Accepts any iterable of strings (file, StringIO, list of lines)
- Parameters: `delimiter=','`, `quotechar='"'`, `dialect=None`
- Returns an iterator; each `next()` call returns one row as `list[str]`
- Handles: multi-char quoted fields, embedded delimiters, doubled-quote escape
- Raises `mycsv.Error` on malformed input (e.g. unclosed quote at EOF)

### writer
- Accepts any file-like object with a `.write()` method
- Parameters: `delimiter=','`, `quotechar='"'`, `lineterminator='\r\n'`, `dialect=None`
- `.writerow(row)` — accepts any iterable
- `.writerows(rows)` — accepts iterable of rows
- Quoting: quotes fields that contain delimiter, quotechar, or lineterminator
- Escapes embedded quotechar by doubling it (`"` → `""`)

### DictReader
- Wraps `reader`; yields `dict` per row
- `fieldnames` auto-detected from first row, or passed explicitly

### DictWriter
- Wraps `writer`; accepts `dict` per row
- `fieldnames` required at construction
- `.writeheader()` writes the fieldnames row

### Dialect
- `register_dialect(name, **fmtparams)` — registers a named dialect
- `list_dialects()` → list of registered names
- `"excel"` dialect pre-registered (comma, `\r\n`, double-quote)

### Errors
- `mycsv.Error` — base exception for all module errors, raised in C via `PyErr_SetString`

---

## Pre-Start Checklist

Before writing a single line of code, verify all of these:

- [ ] **Python dev headers installed**
  ```bash
  sudo apt install python3.13-dev
  # verify:
  python3 -c "import sysconfig; print(sysconfig.get_config_var('INCLUDEPY'))"
  # should print a path that contains Python.h
  ls $(python3 -c "import sysconfig; print(sysconfig.get_config_var('INCLUDEPY'))")/Python.h
  ```

- [ ] **GCC available**
  ```bash
  gcc --version   # already confirmed: 14.2.0 ✓
  ```

- [ ] **pip + setuptools available**
  ```bash
  python3 -m pip --version
  python3 -m pip install --upgrade setuptools wheel
  ```

- [ ] **pytest installed**
  ```bash
  python3 -m pytest --version
  # if missing: pip install pytest
  ```

- [ ] **Editable install works** (do this after Step 1)
  ```bash
  cd mycsv/
  pip install -e . --no-build-isolation
  python3 -c "import _mycsv; print('C extension loaded OK')"
  ```

- [ ] **VSCode C/C++ extension installed** (for syntax highlighting in `_mycsv.c`)
  - Extension ID: `ms-vscode.cpptools`

- [ ] **Read the CPython docs bookmark** — keep this open while coding:
  - https://docs.python.org/3/c-api/index.html
  - Especially: "Concrete Objects" and "Exception Handling"
