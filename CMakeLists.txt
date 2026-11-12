cmake_minimum_required(VERSION 4.3)
project(breachengine C)

message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C compiler version: ${CMAKE_C_COMPILER_VERSION}")

set(CMAKE_C_STANDARD 17)

include(CheckIPOSupported)

check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

if(ipo_supported)
    message(STATUS "LTO enabled")
else()
    message(WARNING "LTO not supported: ${ipo_error}")
endif()

add_executable(breachengine
        src/engine.c
        src/types.h
        src/board.h
        src/board.c
        src/attacks.c
        src/attacks.h
        src/movegen.c
        src/eval.c
        src/eval.h
        src/uci.c
        src/uci.h
        src/search.c
        src/search.h
        src/weights.h
        src/weights.c
        src/tt.c
        src/tt.h
        src/staticanalysis.c
        src/staticanalysis.h
)

target_compile_options(breachengine PRIVATE
        $<$<CONFIG:Release>:-Ofast -flto=auto>
        $<$<CONFIG:ReleaseDebug>:-Ofast>
)

target_compile_definitions(breachengine PRIVATE
        $<$<CONFIG:Debug>:BREACH_DEBUG>
        $<$<CONFIG:ReleaseDebug>:BREACH_DEBUG>
)

if(ipo_supported)
    set_property(TARGET breachengine PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

set(WEIGHTS_FILE ${CMAKE_SOURCE_DIR}/data/weights.txt)

target_compile_definitions(breachengine PRIVATE WEIGHTS_FILE=\"${WEIGHTS_FILE}\")