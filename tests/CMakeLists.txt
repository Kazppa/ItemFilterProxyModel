cmake_minimum_required(VERSION 3.18)

find_package(Qt6 COMPONENTS Core Gui Test REQUIRED)

add_executable(ItemFilterProxyModelTests
    main.cpp
    ColumnIteratorTest.h
    ColumnIteratorTest.cpp
)

target_include_directories(ItemFilterProxyModelTests PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(ItemFilterProxyModelTests PRIVATE
    Qt::Gui
    Qt6::Test
    ItemFilterProxyModel
)

add_test(NAME ItemFilterProxyModelTests
    COMMAND ItemFilterProxyModelTests)