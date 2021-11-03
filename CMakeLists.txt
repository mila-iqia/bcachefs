cmake_minimum_required(VERSION 3.5)

project(bcachefs LANGUAGES C)

include_directories(bcachefs/ ./)

add_executable(bch main.c
    bcachefs/bcachefs.c
    bcachefs/bcachefs_iterator.c
    bcachefs/utils.c
    libbenzina/bcachefs.c
    libbenzina/siphash.c
)
