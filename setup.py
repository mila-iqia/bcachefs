# setup.py
from setuptools import Extension, find_packages, setup

bcachefs_module = Extension(
    name="bcachefs.c_bcachefs",
    sources=["bcachefs/bcachefs.c",
             "bcachefs/bcachefsmodule.c"],
    include_dirs=["bcachefs/"]
)

setup(
    name="bcachefs",
    version="0.1.5",
    author="Satya Ortiz-Gagné",
    url="",
    packages=find_packages(),
    install_requires=["numpy"],
    tests_require=["pytest"],
    ext_modules=[bcachefs_module]
)
