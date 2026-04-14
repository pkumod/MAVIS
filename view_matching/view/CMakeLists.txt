set(ANSWER_GRAPH_SRC 
    storage/super_query_graph.cpp
    storage/mat_view.cpp
    mat_view_builder.cpp
    edge_iterator.cpp
    mat_view_updater.cpp
    )



add_library(MatViewLib SHARED ${ANSWER_GRAPH_SRC})
add_subdirectory(partition)
add_subdirectory(storage)
add_subdirectory(materialization)

target_link_libraries(MatViewLib PRIVATE my_utils PartitionLib StorageLib MaterializationLib)
target_include_directories(MatViewLib PUBLIC 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../utils/
                            ${CMAKE_CURRENT_SOURCE_DIR}/partition
                            ${CMAKE_CURRENT_SOURCE_DIR}/storage
                            ${CMAKE_CURRENT_SOURCE_DIR}/materialization
                            ${CMAKE_SOURCE_DIR}/configuration)