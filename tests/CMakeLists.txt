find_package(Qt6 COMPONENTS Test REQUIRED)

add_executable(ItemFilterProxyModelUnitTests
    main.cpp
    ColumnIteratorTest.h
    ColumnIteratorTest.cpp
)

target_link_libraries(ItemFilterProxyModelUnitTests PRIVATE Qt6::Test ItemFilterProxyModel)
target_include_directories(ItemFilterProxyModelUnitTests PRIVATE ${PROJECT_SOURCE_DIR}/src)

add_test(NAME ItemFilterProxyModelUnitTests
    COMMAND ItemFilterProxyModelUnitTests)