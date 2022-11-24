cmake_minimum_required(VERSION 3.18)

find_package(Qt6 COMPONENTS Core Gui Widgets Test REQUIRED)

add_executable(ItemFilterProxyModelExample
        main.cpp
        ExampleWidget.h
        ExampleWidget.cpp
        ExampleItemModel.h
        ExampleItemModel.cpp
        ExampleItemFilterProxyModel.h
        ExampleItemFilterProxyModel.cpp
        ExampleTreeView.h
        ExampleTreeView.cpp)

target_link_libraries(ItemFilterProxyModelExample PRIVATE
        Qt::Widgets
        Qt::Gui
        Qt6::Test
        ItemFilterProxyModel
)