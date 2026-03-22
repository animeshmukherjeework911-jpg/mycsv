# mycsv.py — Python wrapper layer
# You will fill this in during the project steps.
# It will import _mycsv (the C extension) and expose a clean public API.
import _mycsv

# Error is defined in the C extension to avoid a circular import:
# if Error lived here, _mycsv would need to import mycsv to get it,
# but mycsv imports _mycsv — deadlock. C extension owns the class.
Error = _mycsv.Error


class reader:
    def __init__(self, iterable, delimiter=",", quotechar='"', dialect=None):
        
        if dialect is not None:
            d = get_dialect(dialect) if isinstance(dialect, str) else dialect
            delimiter = getattr(d, "delimiter", delimiter)
            quotechar = getattr(d, "quotechar", quotechar)
        
        # Validate delimiter and quotechar via the C layer so the error
        # goes through mycsv_Error consistently with all other parser errors.
        _mycsv.parse_line("", delimiter, quotechar)

        self._lines = iter(iterable)
        self.delimiter = delimiter
        self.quotechar = quotechar
        self.line_num = 0

    def __iter__(self):
        return self
    
    def __next__(self):
        while True:
            line = next(self._lines)
            self.line_num += 1
            line = line.rstrip("\r\n")

            if line == "":
                continue 

            while line.count(self.quotechar) % 2 != 0:
                try:
                    continuation = next(self._lines)
                    self.line_num += 1
                    line = line + "\n" + continuation.rstrip("\r\n")
                except StopIteration:
                    break 
            try:
                return _mycsv.parse_line(line, self.delimiter, self.quotechar)
            except Error:
                raise

class writer:

    def __init__(self, fileobj, delimiter=",", quotechar='"',
                 lineterminator="\r\n", dialect=None):

        if dialect is not None:
            d = get_dialect(dialect) if isinstance(dialect, str) else dialect
            delimiter = getattr(d, "delimiter", delimiter)
            quotechar = getattr(d, "quotechar", quotechar)
            lineterminator = getattr(d, "lineterminator", lineterminator)

        # Validate delimiter and quotechar via the C layer — consistent with reader.
        _mycsv.format_row([], delimiter=delimiter, quotechar=quotechar)

        self._file = fileobj
        self.delimiter = delimiter
        self.quotechar = quotechar
        self.lineterminator = lineterminator

    def writerow(self, row):
        fields = ["" if f is None else (f if isinstance(f, str) else str(f)) for f in row]

        line = _mycsv.format_row(
            fields,
            delimiter=self.delimiter,
            quotechar=self.quotechar,
            lineterminator=self.lineterminator
        )
        self._file.write(line)
        return len(line)

    def writerows(self, rows):
        for row in rows:
            self.writerow(row)

class DictReader:
    def __init__(self, f, fieldnames=None, restkey=None, restval=None, delimiter=",", quotechar='"', dialect=None):
        self._reader = reader(f, delimiter=delimiter, quotechar=quotechar, dialect=dialect)
        self.fieldnames = fieldnames
        self.rest_key = restkey
        self.rest_val = restval

    @property
    def line_num(self):
        return self._reader.line_num
    
    def __iter__(self):
        return self 
    
    def __next__(self):
        if self.fieldnames is None:
            self.fieldnames = next(self._reader)
        
        row = next(self._reader)
        d = dict(zip(self.fieldnames, row))

        if len(row) < len(self.fieldnames):
            for key in self.fieldnames[len(row):]:
                d[key] = self.rest_val
        elif len(row) > len(self.fieldnames):
            d[self.rest_key] = row[len(self.fieldnames):]

        return d 
    
class DictWriter:
    def __init__(self, f, fieldnames, restval="", 
                 extrasaction="raise", delimiter=",", quotechar='"',
                 lineterminator="\r\n", dialect=None):
        self.fieldnames = fieldnames
        self.restval = restval
        self.extrasaction = extrasaction
        self._writer = writer(f, delimiter=delimiter, 
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

class Dialect:
    """Base class for CSV dialects"""

    delimiter = ","
    quotechar = '"'
    doublequote = True 
    skipinitialspace = False 
    lineterminator = "\r\n"
    quoting = 0 # QUOTE_MINIMAL 

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        if hasattr(cls, "name"):
            register_dialect(cls.name, cls)


_dialect_registry = {}

def register_dialect(name, dialect_cls=None, **fmtparams):
    if dialect_cls is None:
        dialect_cls = type(name, (Dialect, ), fmtparams)

    _dialect_registry[name] = dialect_cls

def unregister_dialect(name):
    if name not in _dialect_registry:
        raise Error(f"unknown dialect: {name!r}") # if name has special character like \r etc, it is printed in raw form using {name!r}
    del _dialect_registry[name]

def list_dialects():
    return list(_dialect_registry.keys())

def get_dialect(name):
    if name not in _dialect_registry:
        raise Error(f"unknown dialect: {name!r}")
    return _dialect_registry[name]

class excel(Dialect):
    name = "excel"
    delimiter = ","
    quotechar = '"'
    lineterminator = "\r\n"

class excel_tab(Dialect):
    name = "excel-tab"
    delimiter = "\t"
    quotechar = '"'
    lineterminator = "\r\n"