cmake_minimum_required(VERSION 3.20)
project(NanoChess C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

include_directories(include)

# Ensure C23 / C2x mode across all compilers
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-std=c2x -O3 -flto)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(MSVC)
        add_compile_options(-Xclang -std=c23 /O1 /Os /Gy /Gw -flto /clang:-fno-unwind-tables /clang:-fno-asynchronous-unwind-tables)
        add_link_options(/OPT:REF /OPT:ICF)
    else()
        add_compile_options(-std=c23 -O3 -flto)
    endif()
elseif(MSVC)
    add_compile_options(/std:clatest /experimental:c11atomics /O2 /Oi /Ot)
endif()

set(ENGINE_SOURCES
    src/zobrist.c
    src/attacks.c
    src/board.c
    src/movegen.c
    src/evaluate.c
    src/tt.c
    src/search.c
    src/uci.c
    src/book.c
)

add_executable(nanochess
    src/main.c
    ${ENGINE_SOURCES}
)

add_executable(board
    tests/board.c
    ${ENGINE_SOURCES}
)

add_executable(perft
    tests/perft.c
    ${ENGINE_SOURCES}
)

add_executable(eval
    tests/eval.c
    ${ENGINE_SOURCES}
)

add_executable(search
    tests/search.c
    ${ENGINE_SOURCES}
)

add_executable(kaufman
    tests/kaufman.c
    ${ENGINE_SOURCES}
)

add_executable(book_compiler
    tools/book_compiler.c
    src/zobrist.c
    src/attacks.c
    src/board.c
    src/movegen.c
)

add_executable(openings
    tools/openings.c
    src/zobrist.c
    src/attacks.c
    src/board.c
    src/movegen.c
    src/book.c
)

if(NOT MSVC)
    target_link_libraries(nanochess PRIVATE m)
    target_link_libraries(book_compiler PRIVATE m)
    target_link_libraries(openings PRIVATE m)
    target_link_libraries(board PRIVATE m)
    target_link_libraries(perft PRIVATE m)
    target_link_libraries(eval PRIVATE m)
    target_link_libraries(search PRIVATE m)
    target_link_libraries(kaufman PRIVATE m)
endif()

enable_testing()
add_test(NAME board COMMAND board)
add_test(NAME perft COMMAND perft)
add_test(NAME eval COMMAND eval)
add_test(NAME search COMMAND search)
add_test(NAME kaufman COMMAND kaufman)
