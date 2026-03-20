"""
test_mycsv.py — full test suite for the mycsv module.

Tests are ordered to match the 10-step build plan so you can run them
incrementally with:  pytest -k "step1"  etc.

Run all:  pytest tests/
"""
import io
import pytest

# ---------------------------------------------------------------------------
# Import guard — gives a clear message if the C extension isn't built yet
# ---------------------------------------------------------------------------
try:
    import mycsv
except ImportError as exc:
    pytest.skip(
        f"mycsv not importable ({exc}). Build with: pip install -e .",
        allow_module_level=True,
    )


# ---------------------------------------------------------------------------
# Step 1 — Module exists and exposes expected public names
# ---------------------------------------------------------------------------
class TestStep1_ModuleStructure:
    def test_reader_exists(self):
        assert hasattr(mycsv, "reader"), "mycsv.reader not found"

    def test_writer_exists(self):
        assert hasattr(mycsv, "writer"), "mycsv.writer not found"

    def test_error_class_exists(self):
        assert hasattr(mycsv, "Error"), "mycsv.Error not found"
        assert issubclass(mycsv.Error, Exception)


# ---------------------------------------------------------------------------
# Step 2 — reader: basic row iteration
# ---------------------------------------------------------------------------
class TestStep2_ReaderBasic:
    def test_reads_header_row(self, simple_csv_file):
        r = mycsv.reader(simple_csv_file)
        assert next(r) == ["name", "age", "city"]

    def test_reads_data_rows(self, simple_csv_file):
        rows = list(mycsv.reader(simple_csv_file))
        assert rows[1] == ["Alice", "30", "NYC"]
        assert rows[2] == ["Bob", "25", "LA"]

    def test_reader_is_iterator(self, simple_csv_file):
        r = mycsv.reader(simple_csv_file)
        assert iter(r) is r


# ---------------------------------------------------------------------------
# Step 3 — reader: delimiter support
# ---------------------------------------------------------------------------
class TestStep3_Delimiter:
    def test_tab_delimiter(self):
        f = io.StringIO("a\tb\tc\n1\t2\t3\n")
        rows = list(mycsv.reader(f, delimiter="\t"))
        assert rows[0] == ["a", "b", "c"]
        assert rows[1] == ["1", "2", "3"]

    def test_pipe_delimiter(self):
        f = io.StringIO("x|y\n1|2\n")
        rows = list(mycsv.reader(f, delimiter="|"))
        assert rows[0] == ["x", "y"]


# ---------------------------------------------------------------------------
# Step 4 — reader: quoted fields
# ---------------------------------------------------------------------------
class TestStep4_QuotedFields:
    def test_comma_inside_quotes(self, csv_with_quotes):
        rows = list(mycsv.reader(io.StringIO(csv_with_quotes)))
        assert rows[1] == ["1", "hello, world"]

    def test_newline_inside_quotes(self, csv_with_quotes):
        rows = list(mycsv.reader(io.StringIO(csv_with_quotes)))
        assert rows[2] == ["2", "line1\nline2"]

    def test_doubled_quote_escape(self):
        f = io.StringIO('a\n"say ""hi"""\n')
        rows = list(mycsv.reader(f))
        assert rows[1] == ['say "hi"']


# ---------------------------------------------------------------------------
# Step 5 — writer: basic write
# ---------------------------------------------------------------------------
class TestStep5_WriterBasic:
    def test_write_simple_row(self):
        out = io.StringIO()
        w = mycsv.writer(out)
        w.writerow(["name", "age"])
        assert out.getvalue() == "name,age\r\n"

    def test_write_multiple_rows(self):
        out = io.StringIO()
        w = mycsv.writer(out)
        w.writerows([["a", "1"], ["b", "2"]])
        assert out.getvalue() == "a,1\r\nb,2\r\n"


# ---------------------------------------------------------------------------
# Step 6 — writer: quoting
# ---------------------------------------------------------------------------
class TestStep6_WriterQuoting:
    def test_quotes_field_with_comma(self):
        out = io.StringIO()
        w = mycsv.writer(out)
        w.writerow(["hello, world"])
        assert out.getvalue() == '"hello, world"\r\n'

    def test_quotes_field_with_newline(self):
        out = io.StringIO()
        w = mycsv.writer(out)
        w.writerow(["line1\nline2"])
        assert '"line1\nline2"' in out.getvalue()

    def test_doubles_embedded_quote(self):
        out = io.StringIO()
        w = mycsv.writer(out)
        w.writerow(['say "hi"'])
        assert out.getvalue() == '"say ""hi"""\r\n'


# ---------------------------------------------------------------------------
# Step 7 — DictReader
# ---------------------------------------------------------------------------
class TestStep7_DictReader:
    def test_returns_dicts(self, simple_csv_file):
        r = mycsv.DictReader(simple_csv_file)
        row = next(r)
        assert row == {"name": "Alice", "age": "30", "city": "NYC"}

    def test_fieldnames_from_header(self, simple_csv_file):
        r = mycsv.DictReader(simple_csv_file)
        list(r)  # exhaust
        assert r.fieldnames == ["name", "age", "city"]

    def test_explicit_fieldnames(self):
        f = io.StringIO("Alice,30\nBob,25\n")
        r = mycsv.DictReader(f, fieldnames=["name", "age"])
        assert next(r) == {"name": "Alice", "age": "30"}


# ---------------------------------------------------------------------------
# Step 8 — DictWriter
# ---------------------------------------------------------------------------
class TestStep8_DictWriter:
    def test_write_dict_row(self):
        out = io.StringIO()
        w = mycsv.DictWriter(out, fieldnames=["name", "age"])
        w.writeheader()
        w.writerow({"name": "Alice", "age": "30"})
        lines = out.getvalue().splitlines()
        assert lines[0] == "name,age"
        assert lines[1] == "Alice,30"


# ---------------------------------------------------------------------------
# Step 9 — Dialect support
# ---------------------------------------------------------------------------
class TestStep9_Dialect:
    def test_register_and_use_dialect(self):
        mycsv.register_dialect("pipes", delimiter="|", lineterminator="\n")
        out = io.StringIO()
        w = mycsv.writer(out, dialect="pipes")
        w.writerow(["a", "b"])
        assert out.getvalue() == "a|b\n"

    def test_list_dialects(self):
        mycsv.register_dialect("pipes", delimiter="|", lineterminator="\n")
        assert "pipes" in mycsv.list_dialects()

    def test_excel_dialect_exists(self):
        assert "excel" in mycsv.list_dialects()


# ---------------------------------------------------------------------------
# Step 10 — Error handling
# ---------------------------------------------------------------------------
class TestStep10_Errors:
    def test_malformed_quote_raises(self):
        f = io.StringIO('"unclosed\n')
        with pytest.raises(mycsv.Error):
            list(mycsv.reader(f))

    def test_invalid_delimiter_raises(self):
        with pytest.raises((TypeError, ValueError)):
            mycsv.reader(io.StringIO("a,b"), delimiter=",,")
