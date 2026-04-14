add_library(MaterializationLib 
    edge_materializer.cpp
    node_materializer.cpp
    materializer.cpp
    view_filter.cpp
)

target_link_libraries(MaterializationLib StorageLib)
target_include_directories(MaterializationLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../storage/)
