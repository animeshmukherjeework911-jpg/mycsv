# Stage 1 — Project skeleton

Set up the build system, create a minimal C extension module that Python
can import, and add Python stubs so the Step 1 tests pass.
No CSV logic yet — just wiring.

---

## Prerequisites

Before you start, make sure these are available on your machine:

```bash
python3 --version        # 3.10 or higher
python3 -c "import sysconfig; print(sysconfig.get_path('include'))"
#  → should print a path like /usr/include/python3.12
gcc --version            # any modern GCC or Clang
pip --version
```

If the include path is missing, install the Python dev headers:
```bash
# Debian / Ubuntu / WSL
sudo apt install python3-dev

# Fedora / RHEL
sudo dnf install python3-devel
```

---

## Sub-step A — Directory structure

Create the project directories:

```
mycsv/
├── src/
│   ├── _mycsv.c      ← C extension (you'll fill this in)
│   └── mycsv.py      ← Python wrapper (you'll fill this in)
├── tests/
│   ├── conftest.py
│   └── test_mycsv.py
├── stages/           ← the markdown guides you're reading now
├── pyproject.toml
└── setup.py
```

The `tests/` and `stages/` directories already exist in the repo — you
don't need to create them.

---

## Sub-step B — `pyproject.toml`

This tells pip how to build your package. Create it at the project root:

```toml
[build-system]
requires = ["setuptools", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "mycsv"
version = "0.1.0"
description = "A hand-rolled CSV module with a C extension"
requires-python = ">=3.10"

[tool.setuptools.packages.find]
where = ["src"]
```

**Why `[tool.setuptools.packages.find]`?**
Without it, setuptools would scan the project root for Python packages
and might not find `mycsv.py` inside `src/`. This entry tells it to look
under `src/` instead.

---

## Sub-step C — `setup.py`

`pyproject.toml` can't declare C extensions yet. A minimal `setup.py`
handles that:

```python
from setuptools import setup, Extension

_mycsv = Extension(
    name="_mycsv",
    sources=["src/_mycsv.c"],
    extra_compile_args=["-Wall", "-Wextra", "-O2"],
)

setup(ext_modules=[_mycsv])
```

The `name="_mycsv"` means Python will see it as `import _mycsv`.
The leading underscore is a convention for "private C backend".

---

## Sub-step D — Minimal `_mycsv.c`

Create `src/_mycsv.c`. For now, all it does is define an empty module
that Python can import:

```c
#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* Module definition — describes the module to CPython.
 * PyModuleDef_HEAD_INIT is a required sentinel that initialises the
 * internal fields CPython uses to manage the module object.
 * The -1 means this module has no per-interpreter state. */
static PyModuleDef _mycsv_module = {
    PyModuleDef_HEAD_INIT,
    "_mycsv",          /* module name — must match PyInit_<name> below */
    "mycsv C backend", /* module docstring */
    -1,
    NULL               /* method table — you'll add this in Stage 2 */
};

/* Entry point — CPython calls this when you do `import _mycsv`.
 * The function name MUST be PyInit_<module_name>. */
PyMODINIT_FUNC
PyInit__mycsv(void)
{
    return PyModule_Create(&_mycsv_module);
}
```

**`#define PY_SSIZE_T_CLEAN`** — must appear before `#include <Python.h>`.
It tells the Python headers to use `Py_ssize_t` (signed) instead of the
old `int` for size parameters. You'd get deprecation warnings without it.

---

## Sub-step E — Python stubs in `mycsv.py`

The Step 1 tests only check that `mycsv.reader`, `mycsv.writer`, and
`mycsv.Error` exist. Create `src/mycsv.py` with the minimum to satisfy that:

```python
import _mycsv

class Error(Exception):
    pass

def reader(iterable, delimiter=",", quotechar='"', dialect=None):
    raise NotImplementedError

def writer(fileobj, delimiter=",", quotechar='"', lineterminator="\r\n", dialect=None):
    raise NotImplementedError
```

> **Note:** In the final implementation (Stage 10), `Error` moves into
> the C extension to avoid a circular import. For now, defining it in
> Python is the right starting point.

---

## Sub-step F — Build and verify

Create a virtual environment and install the package in editable mode:

```bash
python3 -m venv .venv
source .venv/bin/activate    # Windows: .venv\Scripts\activate
pip install -e .
```

`pip install -e .` compiles `_mycsv.c` and links it into your environment.
You must re-run this every time you change C code.

**Smoke-test the import:**
```python
import _mycsv
import mycsv
print(type(mycsv.Error))   # → <class 'type'>
print(mycsv.reader)        # → <function reader at 0x...>
```

---

## Sub-step G — Run the Step 1 tests

```bash
pytest tests/ -k "TestStep1_" -v
```

Expected output:
```
tests/test_mycsv.py::TestStep1_ModuleStructure::test_reader_exists      PASSED
tests/test_mycsv.py::TestStep1_ModuleStructure::test_writer_exists      PASSED
tests/test_mycsv.py::TestStep1_ModuleStructure::test_error_class_exists PASSED
```

All other tests will fail — that's expected. You haven't implemented
anything yet.

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| `#define PY_SSIZE_T_CLEAN` | Suppress size-type deprecation warnings |
| `PyModuleDef` | Describes the module (name, docstring, method table) |
| `PyModuleDef_HEAD_INIT` | Required sentinel in every `PyModuleDef` |
| `PyMODINIT_FUNC` | Return type + `extern "C"` linkage for the init function |
| `PyInit_<name>` | Entry point CPython calls on `import` |
| `PyModule_Create` | Allocates the module object from the definition |
| `setup.py` + `Extension` | How setuptools knows to compile a `.c` file |
| `pip install -e .` | Editable install — no reinstall needed for Python changes |

---

## Checklist before moving to Stage 2

- [ ] `import _mycsv` works in the Python shell
- [ ] `import mycsv` works — no ImportError
- [ ] `mycsv.reader`, `mycsv.writer`, `mycsv.Error` all exist
- [ ] `pytest -k "TestStep1_"` → 3 tests pass
- [ ] `pip install -e .` compiles without warnings
