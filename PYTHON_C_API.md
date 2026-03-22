# Python C API Glossary

Reference for every Python C API used in `src/_mycsv.c`.

---

## Types

| API | Purpose | Example |
|-----|---------|---------|
| `PyObject *` | Universal pointer type for every Python object in C. Every Python value — `int`, `str`, `list`, `None` — is a `PyObject *` at the C level. Functions return `PyObject *` so they can return any Python type, or `NULL` on error. | `PyObject *result = PyList_New(0);` → equivalent to `result = []` |
| `Py_ssize_t` | Signed integer type used for sizes, lengths, and indices in the C API. Always use this instead of `int` or `size_t` when the API expects it. | `Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);` → equivalent to `n = len(seq)` |
| `PyCFunction` | Type for a C function that implements a Python-callable method. Cast to this when registering functions in a `PyMethodDef` table. | `(PyCFunction)mycsv_parse_line` |

---

## Argument Parsing

| API | Purpose | Example |
|-----|---------|---------|
| `PyArg_ParseTuple(args, fmt, ...)` | Unpacks a positional-only argument tuple into C variables. Returns `0` (false) and sets `TypeError` on mismatch — canonical pattern: `if (!PyArg_ParseTuple(...)) return NULL;`. Format codes: `"s"` → `const char *`, `"i"` → `int`, `"l"` → `long`, `"d"` → `double`, `"O"` → `PyObject *`. | `PyArg_ParseTuple(args, "ss", &line, &delim)` → equivalent to `line, delim = args` (two strings) |
| `PyArg_ParseTupleAndKeywords(args, kwargs, fmt, kwlist, ...)` | Same as `PyArg_ParseTuple` but also handles keyword arguments. `kwlist` is a `NULL`-terminated array of parameter name strings. A `\|` in the format string marks the start of optional arguments. | `PyArg_ParseTupleAndKeywords(args, kwargs, "s\|s", kwlist, &line, &delim)` → equivalent to `def fn(line, delim=",")` |

---

## List Objects

| API | Purpose | Example |
|-----|---------|---------|
| `PyList_New(len)` | Creates a new Python list with `len` pre-allocated slots (pass `0` for an empty list). Returns `NULL` on allocation failure. | `PyList_New(0)` → equivalent to `[]` |
| `PyList_Append(list, item)` | Appends `item` to `list`. Increments `item`'s reference count internally, so the caller must still `Py_DECREF` their own reference afterwards. Returns `0` on success, `-1` on error. | `PyList_Append(result, field)` → equivalent to `result.append(field)` |

---

## Unicode / String Objects

| API | Purpose | Example |
|-----|---------|---------|
| `PyUnicode_FromStringAndSize(buf, len)` | Creates a Python `str` from a C `char *` buffer and explicit byte length. Decodes as UTF-8. Returns `NULL` on failure (sets `MemoryError` or `UnicodeDecodeError`). Useful for slicing a sub-string out of a larger buffer without copying first. | `PyUnicode_FromStringAndSize("foo,bar", 3)` → equivalent to `"foo,bar"[:3]` → `"foo"` |
| `PyUnicode_AsUTF8AndSize(str, &len)` | Extracts a UTF-8 encoded `const char *` from a Python `str` and writes the byte length into `len`. The pointer is valid as long as `str` is alive — do not free it. Returns `NULL` if the object is not a `str`. | `const char *s = PyUnicode_AsUTF8AndSize(item, &field_len)` → equivalent to `s = str(item).encode()` |

---

## Sequence Protocol

| API | Purpose | Example |
|-----|---------|---------|
| `PySequence_Fast(obj, errmsg)` | Converts any iterable `obj` into a list or tuple that supports `O(1)` indexed access. Returns `NULL` and sets `TypeError` with `errmsg` if `obj` is not iterable. Caller owns the reference and must `Py_DECREF` it. | `PySequence_Fast(fields_obj, "must be iterable")` → equivalent to `list(fields_obj)` |
| `PySequence_Fast_GET_SIZE(seq)` | Returns the number of items in a sequence previously obtained from `PySequence_Fast`. No error checking — safe only on a valid `PySequence_Fast` result. | `Py_ssize_t n = PySequence_Fast_GET_SIZE(seq)` → equivalent to `n = len(seq)` |
| `PySequence_Fast_GET_ITEM(seq, i)` | Returns a **borrowed** reference to item `i` of a sequence previously obtained from `PySequence_Fast`. Do not `Py_DECREF` it. No bounds checking. | `PyObject *item = PySequence_Fast_GET_ITEM(seq, i)` → equivalent to `item = seq[i]` |

---

## Reference Counting

| API | Purpose | Example |
|-----|---------|---------|
| `Py_DECREF(obj)` | Decrements the reference count of `obj`. When the count reaches `0`, Python frees the object. Every `PyObject *` you "own" must eventually be decremented; forgetting it causes a memory leak. | After `PyList_Append(list, field)`: `Py_DECREF(field)` — you created `field`, you own it, so decrement after appending (the list now holds its own reference). |
| `Py_INCREF(obj)` | Increments the reference count of `obj`. Use when you want to keep a reference to an object that would otherwise be freed — e.g., storing a module-level object beyond the function that created it. | `Py_INCREF(mycsv_Error)` — keeps the exception class alive for the lifetime of the module. |

---

## Memory Management

| API | Purpose | Example |
|-----|---------|---------|
| `PyMem_Malloc(size)` | Allocates `size` bytes from Python's memory allocator. Prefer over plain `malloc` inside extension code — integrates with Python's memory debugging tools. Returns `NULL` on failure. | `char *buf = PyMem_Malloc(128)` → equivalent to `buf = bytearray(128)` |
| `PyMem_Realloc(ptr, size)` | Resizes a block previously allocated with `PyMem_Malloc` or `PyMem_Realloc`. Returns `NULL` on failure (original `ptr` is still valid in that case — free it separately). | `char *tmp = PyMem_Realloc(buf, buf_size * 2)` — doubles the buffer capacity |
| `PyMem_Free(ptr)` | Frees memory allocated with `PyMem_Malloc` / `PyMem_Realloc`. Must be called exactly once per allocation. | `PyMem_Free(buf)` → equivalent to `del buf` |

---

## Exception Handling

| API | Purpose | Example |
|-----|---------|---------|
| `PyErr_SetString(exc, msg)` | Sets the active Python exception to `exc` with message `msg`. After this call, returning `NULL` from your function propagates the exception to the caller. | `PyErr_SetString(mycsv_Error, "delimiter must be a 1-character string")` → equivalent to `raise mycsv.Error("delimiter must be a 1-character string")` |
| `PyErr_NoMemory()` | Sets `MemoryError` and returns `NULL`. Convenience shorthand for out-of-memory paths. Typically used as `return PyErr_NoMemory();`. | `return PyErr_NoMemory()` → equivalent to `raise MemoryError()` |
| `PyErr_NewException(name, base, dict)` | Creates a new exception class with the given dotted `name`. `base` is the base class (`NULL` → `Exception`). `dict` is extra class attributes (`NULL` for none). Returns a new type object. | `PyErr_NewException("_mycsv.Error", NULL, NULL)` → equivalent to `class Error(Exception): pass` |

---

## Module Initialization

| API | Purpose | Example |
|-----|---------|---------|
| `PyModuleDef_HEAD_INIT` | Required macro that initializes the fixed header fields of a `PyModuleDef` struct. Always the first member. | `PyModuleDef_HEAD_INIT` in `static PyModuleDef _mycsv_module = { PyModuleDef_HEAD_INIT, ... }` |
| `PyMODINIT_FUNC` | Macro that expands to the correct return type and calling convention for a module init function (`PyObject *` with `__declspec(dllexport)` on Windows). The function must be named `PyInit_<modulename>`. | `PyMODINIT_FUNC PyInit__mycsv(void)` → Python calls this when you `import _mycsv` |
| `PyModule_Create(def)` | Creates a module object from a `PyModuleDef`. Returns `NULL` on failure. | `PyObject *m = PyModule_Create(&_mycsv_module)` → equivalent to creating the module namespace |
| `PyModule_AddObject(m, name, obj)` | Adds `obj` to module `m` under the attribute `name`. Steals a reference to `obj` on success (do not `Py_DECREF` afterwards). Returns `0` on success, `-1` on error. | `PyModule_AddObject(m, "Error", mycsv_Error)` → equivalent to `m.Error = mycsv_Error` |
| `PyModule_AddIntConstant(m, name, val)` | Convenience wrapper that adds an integer constant to a module. Equivalent to `PyModule_AddObject` with a freshly-built `int`. | `PyModule_AddIntConstant(m, "QUOTE_MINIMAL", 0)` → equivalent to `m.QUOTE_MINIMAL = 0` |

---

## Method Table Flags

| API | Purpose | Example |
|-----|---------|---------|
| `METH_VARARGS` | Flag indicating the C function accepts positional arguments as a tuple `(PyObject *self, PyObject *args)`. | Combined with `METH_KEYWORDS` to also accept keyword arguments. |
| `METH_KEYWORDS` | Flag indicating the C function also accepts a keyword dict `(PyObject *self, PyObject *args, PyObject *kwargs)`. Must be combined with `METH_VARARGS`. | `METH_VARARGS \| METH_KEYWORDS` → equivalent to `def fn(*args, **kwargs)` |