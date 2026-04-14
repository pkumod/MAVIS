add_library(my_utils 
global_vars.cpp
multilayer_trie.cpp 
MyUtils.cpp
combination.cpp
preprocessor1.cpp 
subgraph_filter.cpp
subgraph_estimator.cpp
)

target_include_directories(
    my_utils PUBLIC ${CMAKE_SOURCE_DIR}/graph 
    ${CMAKE_SOURCE_DIR}/
)