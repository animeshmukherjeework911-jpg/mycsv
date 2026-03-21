# Stage 9 — Dialect registry

Implement `register_dialect`, `unregister_dialect`, and `list_dialects`.
Pre-register the `"excel"` dialect. Pure Python — no new C API.

---

## Sub-step A — Dialect class design

A dialect is just a bundle of formatting parameters. Implement it as a
class with class-level defaults:

Add to `src/mycsv.py`:

```python
class Dialect:
    """Base class for CSV dialects."""
    delimiter       = ","
    quotechar       = '"'
    doublequote     = True
    skipinitialspace = False
    lineterminator  = "\r\n"
    quoting         = 0   # QUOTE_MINIMAL

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        # Auto-register if the subclass has a name attribute
        if hasattr(cls, "name"):
            register_dialect(cls.name, cls)
```

---

## Sub-step B — Registry functions

```python
_dialect_registry = {}

def register_dialect(name, dialect_cls=None, **fmtparams):
    """Register a dialect under `name`.

    Can be called as:
      register_dialect("pipes", delimiter="|")
      register_dialect("myexcel", ExcelDialect)
    """
    if dialect_cls is None:
        # Build a new Dialect subclass from keyword params
        dialect_cls = type(name, (Dialect,), fmtparams)
    _dialect_registry[name] = dialect_cls

def unregister_dialect(name):
    if name not in _dialect_registry:
        raise Error(f"unknown dialect: {name!r}")
    del _dialect_registry[name]

def list_dialects():
    return list(_dialect_registry.keys())

def get_dialect(name):
    if name not in _dialect_registry:
        raise Error(f"unknown dialect: {name!r}")
    return _dialect_registry[name]


# Pre-register the "excel" dialect
class excel(Dialect):
    name            = "excel"
    delimiter       = ","
    quotechar       = '"'
    lineterminator  = "\r\n"

# "excel-tab" variant
class excel_tab(Dialect):
    name            = "excel-tab"
    delimiter       = "\t"
    quotechar       = '"'
    lineterminator  = "\r\n"
```

---

## Sub-step C — Wire dialects into `reader` and `writer`

Update the constructors to apply dialect settings when `dialect` is passed:

```python
class reader:
    def __init__(self, iterable, delimiter=",", quotechar='"', dialect=None):
        if dialect is not None:
            d = get_dialect(dialect) if isinstance(dialect, str) else dialect
            delimiter = getattr(d, "delimiter", delimiter)
            quotechar = getattr(d, "quotechar", quotechar)
        self._lines    = iter(iterable)
        self.delimiter = delimiter
        self.quotechar = quotechar
        self.line_num  = 0
        # ... rest unchanged
```

Apply the same pattern to `writer.__init__`.

---

## Sub-step D — Smoke-test

```python
import io, mycsv

print(mycsv.list_dialects())   # ['excel', 'excel-tab']

mycsv.register_dialect("pipes", delimiter="|")
print(mycsv.list_dialects())   # ['excel', 'excel-tab', 'pipes']

buf = io.StringIO("a|b|c\nd|e|f\n")
rows = list(mycsv.reader(buf, dialect="pipes"))
print(rows)  # [['a', 'b', 'c'], ['d', 'e', 'f']]

mycsv.unregister_dialect("pipes")
print(mycsv.list_dialects())   # ['excel', 'excel-tab']
```

---

## Sub-step E — Run tests

```bash
pytest tests/ -k "TestStep9_" -v
```

---

## What you learned in this stage

| Concept | Where used |
|---|---|
| Class as a configuration bundle | `Dialect` subclasses carry formatting params |
| `__init_subclass__` hook | Auto-register dialect subclasses |
| `type(name, bases, attrs)` | Dynamically creating a class from kwargs |
| Named dialect lookup | `get_dialect("excel")` resolves string to class |

---

## Checklist before moving to Stage 10

- [ ] `list_dialects()` returns `["excel", "excel-tab"]` by default
- [ ] `register_dialect("pipes", delimiter="|")` works
- [ ] `unregister_dialect("pipes")` removes it; raises `Error` if not found
- [ ] `reader(f, dialect="excel")` uses comma delimiter
- [ ] `reader(f, dialect="excel-tab")` uses tab delimiter
- [ ] `pytest -k "TestStep9_"` passes
