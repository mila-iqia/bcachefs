cmake_minimum_required(VERSION 3.5)

project(benzcachefs LANGUAGES C)

include_directories(bcachefs/)

add_executable(bch main.c
    bcachefs/bcachefs.c
)
