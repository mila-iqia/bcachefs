cmake_minimum_required(VERSION 3.5)

project(benzcachefs LANGUAGES C)

include_directories(src/)

add_executable(benzcachefs main.c
    src/libbenzina/bcachefs/bcachefs.c
)
