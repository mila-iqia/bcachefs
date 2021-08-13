cmake_minimum_required(VERSION 3.5)

project(benzcachefs LANGUAGES C)

include_directories(
    $ENV{PYTHON_INCLUDE}
    src/)
link_directories($ENV{PYTHON_LIB})

link_libraries(libpython3.7m.a)
add_executable(benzcachefs main.c
    src/libbenzina/bcachefs/bcachefs.c
    src/benzina/bcachefs/bcachefsmodule.c
)
