# setup.py
from setuptools import Extension, find_packages, setup
import sys

extra_compile_args = []
libraries = []

# call python setup.py -coverage install to install with coverage enabled.
# and debug symbols; coverage info will be generated in
# bcachefs/build/temp.linux-x86_64-3.8/bcachefs/*.(gcda|gcno)
if "-coverage" in sys.argv:
    print("Compiling with coverage")
    sys.argv.remove("-coverage")

    extra_compile_args = ["-coverage", "-g3", "-O0"]
    libraries = ["gcov"]

bcachefs_module = Extension(
    name="bcachefs.c_bcachefs",
    sources=[
        "bcachefs/bcachefs.c",
        "bcachefs/bcachefs_iterator.c",
        "bcachefs/bcachefsmodule.c",
        "bcachefs/utils.c",
        "libbenzina/bcachefs.c",
        "libbenzina/siphash.c",
    ],
    include_dirs=["bcachefs/", "./"],
    extra_compile_args=extra_compile_args,
    libraries=libraries,
)

setup(
    name="bcachefs",
    version="0.1.10",
    author="Satya Ortiz-Gagné",
    description="Fast Disk Image for HPC",
    url="",
    packages=find_packages(),
    ext_modules=[bcachefs_module],
)
