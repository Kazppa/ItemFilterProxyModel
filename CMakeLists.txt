cmake_minimum_required(VERSION 3.18)
project(ItemFilterProxyModel)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Color make output
SET(CMAKE_COLOR_MAKEFILE ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS Core REQUIRED)

add_library(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    include/${PROJECT_NAME}/ItemFilterProxyModel.h
    src/ItemFilterProxyModel.cpp
    src/ItemFilterProxyModelPrivate.h
    src/ItemFilterProxyModelPrivate.cpp
    src/ProxyIndexInfo.h
    src/ProxyIndexInfo.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt6::Core)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    $<$<CONFIG:Debug>:KAZ_DEBUG>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME}
        PRIVATE
        /W4
        $<$<CONFIG:Release>:/WX>
    )
else()
    target_compile_options(${PROJECT_NAME}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<CONFIG:Release>:-Werror>
    )
endif()

add_subdirectory(example)

enable_testing()
add_subdirectory(tests)