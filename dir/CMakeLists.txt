cmake_minimum_required(VERSION 3.23)
project(dir C)

set(CMAKE_C_STANDARD 11)

add_executable(dir
        dir/dir2/c.c
        dir/a.c
        dir/b.c
        main.c)
