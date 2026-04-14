add_library(StorageLib mat_view.cpp 
super_query_graph.cpp
super_data_node.cpp
super_data_edge.cpp)
# Only add local and other needed include directories; graph include is now propagated by target_link_libraries
target_include_directories(StorageLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils
    ${PROJECT_SOURCE_DIR}/configuration
    ${PROJECT_SOURCE_DIR}/graph
)

# target_link_libraries(
#     StorageLib PUBLIC graph
# )