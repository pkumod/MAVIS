set(GRAPH_SRC
        graph.cpp)

add_library(graph SHARED
        ${GRAPH_SRC})

# Propagate the include directory to consumers of the graph target
target_include_directories(graph PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)


target_link_libraries(graph utility)
