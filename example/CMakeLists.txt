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

find_package(Qt6 COMPONENTS Test REQUIRED)

target_link_libraries(ItemFilterProxyModelExample PRIVATE Qt6::Test ItemFilterProxyModel)