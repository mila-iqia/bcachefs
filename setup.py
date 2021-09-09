# setup.py
from setuptools import Extension, find_packages, setup

bcachefs_module = Extension(
    name="benzina.c_bcachefs",
    sources=["src/libbenzina/bcachefs/bcachefs.c",
             "src/benzina/bcachefs/bcachefsmodule.c"],
    include_dirs=["src/"]
)

setup(
    name="benzcachefs",
    version="0.1.0",
    author="Satya Ortiz-Gagné",
    url="",
    packages=find_packages("src"),
    package_dir={"": "src"},
    install_requires=["numpy"],
    tests_require=["pytest"],
    ext_modules=[bcachefs_module]
)
