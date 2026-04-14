add_library(PartitionLib
    finergrained_decomposer.cpp
    simple_tree_partition.cpp
    tree_partitioner.cpp
)

target_link_libraries(PartitionLib PRIVATE my_utils)
target_include_directories(PartitionLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utils
    ${CMAKE_SOURCE_DIR}/configuration
    ${CMAKE_SOURCE_DIR}/graph
)
