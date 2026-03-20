"""
setup.py — needed alongside pyproject.toml to declare the C extension.
setuptools can't express Extension() objects in pyproject.toml yet.
"""
from setuptools import setup, Extension

_mycsv = Extension(
    name="_mycsv",                  # the .so will be named _mycsv.cpython-3xx.so
    sources=["src/_mycsv.c"],       # source files to compile
    extra_compile_args=["-Wall", "-Wextra", "-O2"],
)

setup(
    ext_modules=[_mycsv],
)
