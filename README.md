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
PYTHON_C_API.md        Glossary of every C API function used in this project
```

---

## Prerequisites

```bash
python3 --version     # 3.10 or higher required
gcc --version         # any modern GCC or Clang
pip --version
```

You also need the Python development headers (`Python.h`):

```bash
# Debian / Ubuntu / WSL
sudo apt install python3-dev

# Fedora / RHEL
sudo dnf install python3-devel

# macOS (Xcode CLI tools includes them)
xcode-select --install
```

Verify:
```bash
python3 -c "import sysconfig; print(sysconfig.get_path('include'))"
# → /usr/include/python3.12  (or similar)
```

---

## Quick start

```bash
git clone <this-repo>
cd mycsv
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
pytest tests/ -v
```

---

## How to follow along stage by stage

### Option A — Build it yourself (recommended)

Follow `stages/STAGE1.md` through `stages/STAGE10.md`. Each guide tells you exactly
what to write, gives you a smoke-test to verify it, and has a checklist before
moving to the next stage.

```bash
# After pip install -e ., start here:
pytest tests/ -k "TestStep1_" -v

# Implement Stage 2, rebuild, then:
pytest tests/ -k "TestStep1_ or TestStep2_" -v

# Keep going — each guide ends with a checklist.
```

Re-compile after every C change:
```bash
pip install -e . --no-build-isolation
```

### Option B — Browse reference implementations

The `step-by-step` branch contains a `src_step_by_step/` folder with a snapshot
of `_mycsv.c` and `mycsv.py` at every stage:

```bash
git checkout step-by-step

# Read the reference for any stage
cat src_step_by_step/stage3/_mycsv.c

# Diff two stages to see exactly what changed
diff src_step_by_step/stage3/_mycsv.c src_step_by_step/stage4/_mycsv.c
diff src_step_by_step/stage3/mycsv.py src_step_by_step/stage4/mycsv.py

git checkout master   # return to the complete implementation
```

Try to implement a stage yourself first (Option A), then compare with the
reference to see what you missed.

---

## Stage map

| Stage | What you implement | C API introduced | Tests |
|---|---|---|---|
| 1 | Module skeleton, `pyproject.toml`, `setup.py`, Python stubs | `PyModuleDef`, `PyMODINIT_FUNC` | `TestStep1_` (3 tests) |
| 2 | `parse_line` — split on delimiter, no quoting | `PyArg_ParseTuple`, `PyList_New`, `PyUnicode_FromStringAndSize`, `Py_DECREF` | `TestStep2_` |
| 3 | Keyword args on `parse_line`; `mycsv.reader` class in Python | `PyArg_ParseTupleAndKeywords`, `METH_KEYWORDS` | `TestStep3_` |
| 4 | Quote state machine in C — `FIELD_START` → `IN_QUOTED` | `PyMem_Malloc/Realloc/Free`, `BUF_APPEND` macro, `PyErr_SetString` | `TestStep4_` |
| 5 | `format_row` basic — join fields with delimiter | `PySequence_Fast`, `PyUnicode_AsUTF8AndSize` | `TestStep6_` (partial) |
| 6 | `format_row` full quoting modes + `mycsv.writer` | quoting constants, `PyModule_AddIntConstant` | `TestStep6_` |
| 7 | Harden `reader` (skip empty lines, multi-line fields) + `writerows` | — | `TestStep7_` |
| 8 | `DictReader`, `DictWriter` | — | `TestStep8_` |
| 9 | `Dialect` class, `register_dialect`, `excel` / `excel-tab` | — | `TestStep9_` |
| 10 | `mycsv.Error` in C, input validation, edge-case parity | `PyErr_NewException`, `PyModule_AddObject` | All 44 |

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

See `PYTHON_C_API.md` for a full glossary.

---

## What's next

`CHALLENGES_AHEAD.md` lists 14 extensions grouped by what they teach:

- **Spec compliance** — `skipinitialspace`, `QUOTE_NONNUMERIC` read mode, strict RFC 4180
- **Performance** — benchmark vs stdlib, SIMD delimiter scanning (SSE2/AVX2), `mmap` bulk reader
- **New features** — dialect sniffer, async reader, typed `DictReader` with schema
- **Systems** — full C `reader` class with `PyTypeObject` / `tp_iternext` (how stdlib `csv.reader` is actually built)

Recommended path if you want to stay in the C extension track:
```
skipinitialspace → line-stitching in C → SIMD scanning → mmap reader → full C reader class
```
