# Stage 7 — `mycsv.reader` + `mycsv.writer` full Python wrappers

Polish the reader and writer classes, add `line_num` tracking, handle
edge cases (empty lines, BOM), and verify they are drop-in compatible
with stdlib `csv`.

---

## Sub-step A — Harden `mycsv.reader`

Update `src/mycsv.py`:

```python
class reader:
    def __init__(self, iterable, delimiter=",", quotechar='"', dialect=None):
        self._lines    = iter(iterable)
        self.delimiter = delimiter
        self.quotechar = quotechar
        self.line_num  = 0

    def __iter__(self):
        return self

    def __next__(self):
        while True:
            line = next(self._lines)   # propagates StopIteration
            self.line_num += 1
            line = line.rstrip("\r\n")
            # Skip completely blank lines (mirrors stdlib behaviour)
            if line == "":
                continue
            try:
                return _mycsv.parse_line(line, self.delimiter, self.quotechar)
            except ValueError as e:
                raise Error(str(e)) from e
```

Key changes:
- Blank lines are skipped (stdlib `csv` does the same)
- `ValueError` from C is re-raised as `mycsv.Error`

---

## Sub-step B — Harden `mycsv.writer`

```python
class writer:
    def __init__(self, fileobj, delimiter=",", quotechar='"',
                 lineterminator="\r\n", dialect=None):
        self._file          = fileobj
        self.delimiter      = delimiter
        self.quotechar      = quotechar
        self.lineterminator = lineterminator

    def writerow(self, row):
        fields = [str(f) if not isinstance(f, str) else f for f in row]
        line = _mycsv.format_row(
            fields,
            delimiter=self.delimiter,
            quotechar=self.quotechar,
            lineterminator=self.lineterminator,
        )
        self._file.write(line)
        return len(line)

    def writerows(self, rows):
        for row in rows:
            self.writerow(row)
```

Key changes:
- Non-string fields are coerced to `str` (stdlib `csv` does this)
- `writerow` returns number of characters written

---

## Sub-step C — Multi-line field support in `reader`

`parse_line` operates on a single C string — it has no concept of newlines
spanning multiple input lines. Multi-line support lives entirely in the
Python `reader` layer: stitch continuation lines together before calling
`parse_line`.

**Heuristic:** a line is "incomplete" (inside a quoted field) when the count
of quotechar characters seen so far is odd. Each `""` escape contributes two
quotes (even), each opening or closing quote contributes one (odd).

Update `__next__` in `src/mycsv.py`:

```python
def __next__(self):
    while True:
        line = next(self._lines)   # propagates StopIteration
        self.line_num += 1
        line = line.rstrip("\r\n")
        if line == "":
            continue
        # Stitch continuation lines for multi-line quoted fields.
        # Odd quotechar count → we are inside a quoted field.
        while line.count(self.quotechar) % 2 != 0:
            try:
                continuation = next(self._lines)
                self.line_num += 1
                line = line + "\n" + continuation.rstrip("\r\n")
            except StopIteration:
                break  # let parse_line raise the unclosed-quote error
        try:
            return _mycsv.parse_line(line, self.delimiter, self.quotechar)
        except ValueError as e:
            raise Error(str(e)) from e
```

> **Limitation:** the odd/even heuristic breaks if a field contains an
> unbalanced literal quotechar (rare in valid RFC 4180 CSV). For the
> purposes of this project it matches stdlib `csv` behaviour on well-formed
> input.

---

## Sub-step E — Compatibility smoke-test against stdlib

```python
import csv, mycsv, io

ROWS = [
    ["Name", "Age", "City"],
    ["Alice", "30", "New York"],
    ["Bob",   "25", 'LA, "the city"'],
]

# Write with mycsv, read back with stdlib csv
buf = io.StringIO()
mycsv.writer(buf).writerows(ROWS)
buf.seek(0)
result = list(csv.reader(buf))
assert result == ROWS, f"Mismatch: {result}"

# Write with stdlib csv, read back with mycsv
buf2 = io.StringIO()
csv.writer(buf2).writerows(ROWS)
buf2.seek(0)
result2 = list(mycsv.reader(buf2))
assert result2 == ROWS, f"Mismatch: {result2}"

print("Cross-compatibility OK")
```

---

## Sub-step F — Run tests

```bash
pytest tests/ -k "TestStep7_" -v
```

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| Skipping blank lines | Reader robustness — mirrors stdlib |
| Re-raising C exceptions as module errors | `ValueError` → `mycsv.Error` |
| Coercing fields to `str` | Writer accepting mixed-type rows |
| Cross-compatibility testing | Writing with one lib, reading with another |

---

## Checklist before moving to Stage 8

- [ ] Blank lines in input are silently skipped
- [ ] Malformed quoted fields raise `mycsv.Error` (not `ValueError`)
- [ ] `writerow` coerces integers/floats to strings
- [ ] Multi-line quoted fields are stitched and parsed correctly
- [ ] Cross-compatibility test passes
- [ ] `pytest -k "TestStep7_"` passes  → runs `TestStep7_MultilineFields`
