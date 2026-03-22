# mycsv — Challenges Ahead

`mycsv` is a complete reimplementation of Python's `csv` stdlib module with a
C extension backend. The core is done. Everything below is an extension — each
challenge introduces a new concept while building directly on what you have.

Challenges are grouped by what they teach, not by difficulty.

---

## Level 1 — Spec Compliance

These are gaps between your implementation and RFC 4180 / stdlib behaviour.
Small in scope, high in learning value.

---

### 1.1 `skipinitialspace` support

`Dialect.skipinitialspace` is already defined in your `Dialect` class but
never used in parsing. When `True`, a space immediately after a delimiter
should be stripped before the field value.

```python
# with skipinitialspace=True:
# "Alice, 30, NYC" → ["Alice", "30", "NYC"]  (space after comma stripped)
# "Alice, 30, NYC" → ["Alice", " 30", " NYC"]  (current behaviour)
```

**Where to implement:** inside `mycsv_parse_line` in C, at `FIELD_START` —
if `skipinitialspace` is set and `c == ' '` after a sep, skip it.

**What you learn:** extending an existing state machine, passing a new flag
from Python kwargs down to C.

---

### 1.2 `QUOTE_NONNUMERIC` read behaviour

`QUOTE_NONNUMERIC` (quoting=2) is accepted and stored but does nothing
special during reading. In stdlib `csv`, when this mode is active, the
reader converts all non-quoted fields to `float`.

```python
import csv, io
r = csv.reader(io.StringIO("1,2.5,hello"), quoting=csv.QUOTE_NONNUMERIC)
print(next(r))   # → [1.0, 2.5, 'hello']   ← unquoted fields become float
```

**Where to implement:** in `reader.__next__`, after `_mycsv.parse_line`,
check if `quoting == 2` and attempt `float()` on each field.

**What you learn:** how quoting modes affect both read and write paths,
Python's `float()` edge cases (`"inf"`, `"nan"`, locale decimal separators).

---

### 1.3 RFC 4180 strict mode

Add a `strict=True` dialect parameter that makes the parser raise on any
deviation from RFC 4180:
- Bare `"` inside an unquoted field is currently silently allowed in some
  paths
- Lines not terminated with `\r\n` should warn or raise
- Empty lines should raise instead of being silently skipped

**What you learn:** the difference between lenient parsing (real-world CSVs)
and spec-conformant parsing, and why they're different.

---

## Level 2 — Performance

These challenges are about making the existing implementation faster and
measuring it rigorously.

---

### 2.1 Benchmark suite vs stdlib `csv`

Build a proper benchmark: generate realistic CSV data (mixed quoted/unquoted,
varying field counts, embedded newlines), then measure parse throughput of
`mycsv` vs stdlib `csv` vs `DictReader` on the same data.

```python
# Target metrics:
# - rows/second for parse_line
# - MB/second for reader iteration
# - Memory peak for 1M row file
```

**Tools:** `timeit`, `memory_profiler`, `cProfile`, `pyperf` for stable
measurements.

**Why it matters:** you built the same thing as stdlib. Are you faster,
slower, or the same? The answer tells you whether your C is well-written or
if stdlib's C is doing something you aren't.

---

### 2.2 Move `reader.__next__` line-stitching into C

Currently, the multi-line field stitching (handling `\n` inside quoted
fields) lives in Python:

```python
# mycsv.py — pure Python loop
while line.count(self.quotechar) % 2 != 0:
    continuation = next(self._lines)
    line = line + "\n" + continuation.rstrip("\r\n")
```

The `count()` approach is also fragile — it counts ALL quote chars, not just
unescaped ones, so `"a""b"` (which has 4 quotes, correctly balanced) would
pass, but a field like `"a"""` (3 quotes) would be miscounted.

**Challenge:** implement a `mycsv_stitch_lines` C function that takes an
iterator, reads lines until the quote state machine reaches a clean
`FIELD_START` state at end of line, and returns the stitched string.

**What you learn:** driving a Python iterator from C (`PyIter_Next`),
incremental state machine across multiple C calls.

---

### 2.3 SIMD delimiter scanning in `parse_line_no_quote`

`parse_line_no_quote` scans one byte at a time. For large fields with no
quoting, you can scan 16 or 32 bytes at a time using SSE2/AVX2 intrinsics
to find the delimiter position.

```c
#include <emmintrin.h>   // SSE2

/* Scan 16 bytes at a time for separator character */
__m128i sep_vec = _mm_set1_epi8(sep);
while (p + 16 <= end) {
    __m128i chunk = _mm_loadu_si128((__m128i *)p);
    __m128i match = _mm_cmpeq_epi8(chunk, sep_vec);
    int mask = _mm_movemask_epi8(match);
    if (mask) {
        p += __builtin_ctz(mask);
        break;
    }
    p += 16;
}
```

**What you learn:** SIMD intrinsics, why DuckDB and Polars are fast, the
relationship between data layout and CPU throughput. This is a genuine
performance technique used in production CSV parsers.

---

### 2.4 Memory-mapped file reader

For large CSV files, reading line-by-line with Python's file iterator
involves many small `read()` syscalls and Python string allocations per
line. `mmap` maps the whole file into virtual memory and lets you scan it
as a C array.

```c
/* Instead of iterating lines in Python, give C a raw memory view */
static PyObject *
mycsv_parse_mmap(PyObject *self, PyObject *args)
{
    const char *data;
    Py_ssize_t  size;
    if (!PyArg_ParseTuple(args, "y#", &data, &size))
        return NULL;
    /* scan data[0..size] directly — no Python string allocation per line */
}
```

Expose it as `mycsv.parse_buffer(data: bytes) -> list[list[str]]`.

**What you learn:** `mmap` in Python, `y#` format code in `PyArg_ParseTuple`,
bulk parsing vs line-at-a-time.

---

## Level 3 — New Features

These add new public API surface, each solving a real problem.

---

### 3.1 CSV Sniffer — dialect auto-detection

Implement `mycsv.Sniffer` like stdlib's `csv.Sniffer`. Given a sample of
text, detect the delimiter, quotechar, and whether the first row is a header.

```python
sniffer = mycsv.Sniffer()
dialect = sniffer.sniff(sample)   # → inferred Dialect
has_header = sniffer.has_header(sample)
```

**Algorithm:** count frequency of candidate delimiter chars (`,`, `;`, `\t`,
`|`) outside quoted regions. The most consistent one across rows wins.

**What you learn:** heuristic algorithm design, why "detect the delimiter"
is harder than it sounds (what about `1,000.00` as a number?), statistical
approaches to format inference.

---

### 3.2 Async reader — `AsyncCsvReader`

A reader that works with async file I/O:

```python
async with aiofiles.open("data.csv") as f:
    async for row in mycsv.AsyncCsvReader(f):
        process(row)
```

**Implementation:** wrap `reader` — the hard part is that Python's
`async for` protocol requires `__aiter__` / `__anext__`, and `__anext__`
must be a coroutine that `await`s the underlying async file read.

**What you learn:** the `__aiter__` / `__anext__` protocol, the difference
between sync and async iterators, how asyncio interacts with file I/O
(spoiler: files are not truly async on Linux without `io_uring`).

---

### 3.3 Typed `DictReader` with schema inference

Extend `DictReader` to accept a schema and coerce fields automatically:

```python
r = mycsv.DictReader(f, schema={
    "id":    int,
    "price": float,
    "name":  str,
    "active": lambda x: x.lower() == "true",
})
for row in r:
    print(row["id"] + 1)   # already an int — no manual conversion
```

**What you learn:** Python's `callable` protocol, schema validation patterns,
where type coercion belongs (read layer vs application layer), and the
tradeoffs of making a parser "smart".

---

### 3.4 Parallel CSV parser using `multiprocessing`

For a large CSV file, split it into N chunks by byte offset, parse each
chunk in a separate process, and merge the results. The challenge: chunk
boundaries may fall inside a quoted field.

```python
rows = mycsv.parallel_read("large.csv", workers=4)
```

**Key problem:** finding valid split points. You can't just split at every
`\n` — you need to find a `\n` that is outside a quoted field. This requires
a pre-scan pass.

**What you learn:** multiprocessing with large data, the `mmap` + shared
memory approach, why parallel CSV parsing is harder than parallel line
counting.

---

## Level 4 — Systems

These are production-grade extensions. Each one is a substantial project.

---

### 4.1 Binary / bytes mode

The current implementation only handles `str` (Unicode). Add a parallel
`_mycsv.parse_line_bytes` that operates on `bytes` objects directly, with
explicit encoding parameter.

```python
mycsv.reader(f, encoding="latin-1")   # currently impossible
```

**Why it matters:** real-world CSV files arrive in Latin-1, CP1252, UTF-16
with BOM. The current implementation delegates encoding to Python's file
open, which breaks for files with mixed or unknown encoding.

**What you learn:** `y#` vs `s` format codes in `PyArg_ParseTuple`,
Python's codec system, BOM detection.

---

### 4.2 Thread-safe dialect registry

`_dialect_registry` is a plain Python dict — not thread-safe. Two threads
calling `register_dialect` and `list_dialects` simultaneously can corrupt
it.

**Implementation options:**
1. A `threading.RLock` wrapping all registry mutations
2. Move the registry to C using a `PyDict` protected by the GIL (simpler —
   the GIL already serialises dict operations in CPython)

**What you learn:** thread safety in Python, when the GIL is sufficient,
the difference between CPython thread safety and PyPy/free-threaded safety.

---

### 4.3 C extension for the full `reader` class

Move the entire `reader` class into C — including line iteration, multi-line
field stitching, and state machine — eliminating the Python wrapper layer
entirely.

```c
typedef struct {
    PyObject_HEAD
    PyObject  *source;      /* the underlying iterable */
    char       delimiter;
    char       quotechar;
    int        line_num;
    /* ... state machine state for multi-line fields ... */
} CsvReaderObject;
```

This is how stdlib `csv.reader` is implemented. You would implement:
- `PyTypeObject` with `tp_new`, `tp_init`, `tp_iter`, `tp_iternext`
- `tp_dealloc` for reference cleanup

**What you learn:** `PyTypeObject`, the slot-based object protocol, `tp_iternext`
(the C equivalent of `__next__`), reference counting in a stateful C object.
This is the single largest jump in C API complexity in the whole project.

---

## Summary table

| Challenge | Teaches | Estimated scope |
|---|---|---|
| 1.1 skipinitialspace | Extending state machine | Small |
| 1.2 QUOTE_NONNUMERIC read | Quoting modes end-to-end | Small |
| 1.3 Strict mode | Parser design tradeoffs | Small |
| 2.1 Benchmark suite | Performance measurement | Medium |
| 2.2 Line-stitching in C | PyIter_Next, incremental C state | Medium |
| 2.3 SIMD scanning | CPU intrinsics, throughput | Medium |
| 2.4 mmap reader | Memory-mapped I/O, bulk parsing | Medium |
| 3.1 Sniffer | Heuristic algorithm design | Medium |
| 3.2 AsyncCsvReader | `__aiter__`/`__anext__` protocol | Medium |
| 3.3 Typed DictReader | Schema validation patterns | Medium |
| 3.4 Parallel parser | Multiprocessing, split points | Large |
| 4.1 Bytes mode | Encoding, codec system | Large |
| 4.2 Thread-safe registry | GIL, locking, thread safety | Small–Medium |
| 4.3 Full C reader class | PyTypeObject, tp_iternext | Large |

---

## Recommended order

If you want to stay in the C extension track (aligns with your current work):

```
1.1 → 2.2 → 2.3 → 2.4 → 4.3
```

If you want to branch into async and higher-level Python:

```
1.2 → 3.1 → 3.2 → 3.3
```

If you want to connect this to the backend projects (memprof, mymq, etc.):

```
2.1 → 3.4 → 4.1 → 4.2
```