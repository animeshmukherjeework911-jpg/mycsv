# mycsv

A reimplementation of Python's `csv` stdlib module with a C extension backend.

This isn't a better CSV library. It's a learning project: the goal was to understand
what actually happens when you call `csv.reader()` — how the C extension gets called,
how the state machine handles quoted fields, how Python objects live in C memory.

The finished module is a functional drop-in for stdlib `csv`. It passes 44 tests
including round-trip, edge cases, and dialect support.

---

## What's in here

```
src/
  _mycsv.c     C extension — parse_line, format_row, error class, quoting constants
  mycsv.py     Python layer — reader, writer, DictReader, DictWriter, Dialect

stages/
  STAGE1.md … STAGE10.md    Step-by-step build notes with code and explanations
  STATE_MACHINE.md           Deep dive into the quote-handling state machine

tests/
  test_mycsv.py    44 tests, grouped by step so you can run them incrementally

CHALLENGES_AHEAD.md    Extensions: SIMD scanning, mmap reader, async reader, and more
```

---

## How to follow the stages

Each stage builds on the previous one. The tests are named to match:

```bash
pytest tests/ -k "TestStep1_" -v   # just stage 1 tests
pytest tests/ -k "TestStep4_" -v   # just stage 4 tests
pytest tests/ -v                    # all 44 tests
```

**Setup:**

```bash
python -m venv .venv
source .venv/bin/activate
pip install -e .
```

The `pip install -e .` step compiles `_mycsv.c`. Re-run it after every C change.

---

## Stage map

| Stage | What you implement | C API introduced | Tests that pass |
|---|---|---|---|
| 1 | Module skeleton, `pyproject.toml`, `setup.py` | `PyModuleDef`, `PyMODINIT_FUNC` | `TestStep1_` (3 tests) |
| 2 | `parse_line` — split on delimiter, no quoting | `PyArg_ParseTuple`, `PyList_New`, `PyUnicode_FromStringAndSize`, `Py_DECREF` | `TestStep2_` |
| 3 | `mycsv.reader` class in Python, wires `parse_line` | — | `TestStep3_` |
| 4 | Quote state machine in C — `FIELD_START` → `IN_QUOTED` | `PyMem_Malloc/Realloc/Free`, `BUF_APPEND` macro | `TestStep4_` |
| 5 | `parse_line` keyword args, `PyArg_ParseTupleAndKeywords` | `kwlist[]`, `METH_KEYWORDS` | existing tests |
| 6 | `format_row` — write path with quoting decisions | `PySequence_Fast`, `PyUnicode_AsUTF8AndSize` | `TestStep6_` |
| 7 | Multi-line field stitching in `reader.__next__` | — | `TestStep7_` |
| 8 | `DictReader`, `DictWriter` | — | `TestStep8_` |
| 9 | `Dialect` class, `register_dialect`, `excel`/`excel-tab` | `PyModule_AddIntConstant` | `TestStep9_` |
| 10 | `mycsv.Error` in C, validation, edge-case parity | `PyErr_NewException`, `PyErr_SetString` | All 44 |

---

## Key things the C extension teaches

The interesting part is Stage 4 — the quote-handling state machine. Naive `line.split(",")`
breaks on `"New York, NY"`. The fix is a three-state machine:

```
FIELD_START → see quote      → IN_QUOTED
FIELD_START → see other char → IN_FIELD
IN_QUOTED   → see ""         → stay IN_QUOTED, emit single "
IN_QUOTED   → see " then sep → emit field, go to FIELD_START
```

`STATE_MACHINE.md` has the full transition table and a character-by-character worked
example through `1,"New York, ""the Big Apple""",active`.

The write path (`format_row`) uses a two-phase approach: first decide whether a field
needs quoting (scan for `,`, `"`, `\r`, `\n`), then write. This keeps the write loop
clean and is the same pattern used in CPython's `_csv.c`.

**C API surface covered across all stages:**

`PyArg_ParseTuple` · `PyArg_ParseTupleAndKeywords` · `PyList_New` · `PyList_Append` ·
`PyUnicode_FromStringAndSize` · `PyUnicode_AsUTF8AndSize` · `PySequence_Fast` ·
`PyErr_NewException` · `PyErr_SetString` · `PyErr_NoMemory` ·
`PyMem_Malloc` · `PyMem_Realloc` · `PyMem_Free` ·
`PyModule_AddIntConstant` · `Py_DECREF` · `Py_INCREF`

---

## What's next

`CHALLENGES_AHEAD.md` lists 14 extensions grouped by what they teach:

- **Spec compliance** — `skipinitialspace`, `QUOTE_NONNUMERIC` read mode, strict RFC 4180
- **Performance** — benchmark vs stdlib, SIMD delimiter scanning (SSE2/AVX2), `mmap` bulk reader
- **New features** — dialect sniffer, async reader, typed `DictReader` with schema
- **Systems** — full C `reader` class with `PyTypeObject` / `tp_iternext` (how stdlib `csv.reader` is actually built)

The recommended path if you want to stay in the C extension track:
```
skipinitialspace → line-stitching in C → SIMD scanning → mmap reader → full C reader class
```
