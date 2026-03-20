"""
conftest.py — pytest fixtures shared across all test files.
"""
import io
import pytest


@pytest.fixture
def simple_csv_text():
    """A minimal well-formed CSV string."""
    return "name,age,city\nAlice,30,NYC\nBob,25,LA\n"


@pytest.fixture
def simple_csv_file(simple_csv_text):
    """An in-memory file-like object wrapping simple_csv_text."""
    return io.StringIO(simple_csv_text)


@pytest.fixture
def csv_with_quotes():
    """CSV containing quoted fields (commas and newlines inside quotes)."""
    return 'id,note\n1,"hello, world"\n2,"line1\nline2"\n'


@pytest.fixture
def csv_with_escapes():
    """CSV using backslash escaping instead of doubling."""
    return 'a,b\n"he said \\"hi\\"",plain\n'
