# Stage 8 — `DictReader` + `DictWriter`

Build the dict-based wrappers as a pure Python layer on top of
`mycsv.reader` and `mycsv.writer`. No new C API — this stage is
about Python class design.

---

## Sub-step A — Implement `DictReader`

`DictReader` wraps `reader` and yields one `dict` per row.
Field names come from the first row, or are passed explicitly.

Add to `src/mycsv.py`:

```python
class DictReader:
    def __init__(self, f, fieldnames=None, restkey=None, restval=None,
                 delimiter=",", quotechar='"', dialect=None):
        self._reader    = reader(f, delimiter=delimiter,
                                 quotechar=quotechar, dialect=dialect)
        self.fieldnames = fieldnames   # None means "read from first row"
        self.restkey    = restkey      # key for extra fields not in fieldnames
        self.restval    = restval      # value for missing fields
        self._first_row_read = False

    @property
    def line_num(self):
        return self._reader.line_num

    def __iter__(self):
        return self

    def __next__(self):
        if self.fieldnames is None:
            self.fieldnames = next(self._reader)  # consume header row
        row = next(self._reader)                  # may raise StopIteration
        # Zip fields to names; handle mismatched lengths
        d = dict(zip(self.fieldnames, row))
        if len(row) < len(self.fieldnames):
            for key in self.fieldnames[len(row):]:
                d[key] = self.restval
        elif len(row) > len(self.fieldnames):
            d[self.restkey] = row[len(self.fieldnames):]
        return d
```

---

## Sub-step B — Implement `DictWriter`

`DictWriter` wraps `writer` and accepts `dict` per row.
`fieldnames` is required at construction.

```python
class DictWriter:
    def __init__(self, f, fieldnames, restval="", extrasaction="raise",
                 delimiter=",", quotechar='"', lineterminator="\r\n",
                 dialect=None):
        self.fieldnames   = fieldnames
        self.restval      = restval
        self.extrasaction = extrasaction
        self._writer      = writer(f, delimiter=delimiter,
                                   quotechar=quotechar,
                                   lineterminator=lineterminator,
                                   dialect=dialect)

    def writeheader(self):
        self._writer.writerow(self.fieldnames)

    def writerow(self, rowdict):
        if self.extrasaction == "raise":
            extras = set(rowdict) - set(self.fieldnames)
            if extras:
                raise ValueError(f"dict contains fields not in fieldnames: {extras}")
        row = [rowdict.get(f, self.restval) for f in self.fieldnames]
        self._writer.writerow(row)

    def writerows(self, rowdicts):
        for d in rowdicts:
            self.writerow(d)
```

---

## Sub-step C — Smoke-test

```python
import io, mycsv

# DictWriter round-trip
buf = io.StringIO()
dw = mycsv.DictWriter(buf, fieldnames=["name", "age", "city"])
dw.writeheader()
dw.writerows([
    {"name": "Alice", "age": "30", "city": "New York"},
    {"name": "Bob",   "age": "25", "city": "LA"},
])
buf.seek(0)

dr = mycsv.DictReader(buf)
for row in dr:
    print(row)
# {'name': 'Alice', 'age': '30', 'city': 'New York'}
# {'name': 'Bob',   'age': '25', 'city': 'LA'}

# Missing field → restval
buf2 = io.StringIO("name,age\nAlice\n")
for row in mycsv.DictReader(buf2, restval="N/A"):
    print(row)
# {'name': 'Alice', 'age': 'N/A'}
```

---

## Sub-step D — Run tests

```bash
pytest tests/ -k "TestStep8_" -v
```

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| Lazy header detection | `fieldnames=None` reads first row on demand |
| `restkey` / `restval` | Handling row length mismatches |
| `extrasaction="raise"` | Guarding against unexpected dict keys |
| `writeheader()` | Writing fieldnames as the first row |

---

## Checklist before moving to Stage 9

- [ ] `DictReader` auto-detects fieldnames from first row
- [ ] `DictReader(f, fieldnames=[...])` skips header detection
- [ ] Missing fields filled with `restval`
- [ ] Extra fields stored under `restkey`
- [ ] `DictWriter.writeheader()` writes fieldnames row
- [ ] `DictWriter` raises `ValueError` on extra keys when `extrasaction="raise"`
- [ ] `pytest -k "TestStep8_"` passes  → runs `TestStep8_DictReader` and `TestStep8_DictWriter`
