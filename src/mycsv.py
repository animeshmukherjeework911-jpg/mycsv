# mycsv.py — Python wrapper layer
# You will fill this in during the project steps.
# It will import _mycsv (the C extension) and expose a clean public API.
import _mycsv

class Error(Exception):
    pass 

class reader:
    def __init__(self, iterable, delimiter=",", quotechar='"', dialect=None):
        self._lines = iter(iterable)
        self.delimiter = delimiter
        self.quotechar = quotechar 
        self.line_num = 0

    def __iter__(self):
        return self
    
    def __next__(self):
        line = next(self._lines)
        self.line_num += 1
        line = line.rstrip("\r\n")
        return _mycsv.parse_line(line, self.delimiter, self.quotechar)

# def reader(iterable, delimiter=",", quotechar='"', dialect=None):
#     raise NotImplementedError

def writer(fileobj, delimiter=",", quotechar='"', lineterminator="\r\n", dialect=None):
    raise NotImplementedError