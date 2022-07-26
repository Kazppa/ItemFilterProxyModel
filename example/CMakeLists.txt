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

target_link_libraries(ItemFilterProxyModelExample PUBLIC ItemFilterProxyModel)