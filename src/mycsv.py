# mycsv.py — Python wrapper layer
# You will fill this in during the project steps.
# It will import _mycsv (the C extension) and expose a clean public API.
import _mycsv

class Error(Exception):
    pass 

def reader(iterable, delimiter=",", quotechar='"', dialect=None):
    raise NotImplementedError

def writer(fileobj, delimiter=",", quotechar='"', lineterminator="\r\n", dialect=None):
    raise NotImplementedError