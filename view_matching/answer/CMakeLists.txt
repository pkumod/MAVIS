add_library(RewriteLib SHARED 
rewrite_query.cpp
enumerate.cpp
encoder.cpp 
query_plan_generator1.cpp 
query_plan_generator.cpp
rm_filter.cpp
preprocessor.cpp
planner.cpp
)

add_library(SimpleMatchLib SHARED
    simple_matching.h 
    simple_matching.cpp
)

target_link_libraries(SimpleMatchLib graph my_utils MatViewLib)
target_include_directories(SimpleMatchLib PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/
    ${CMAKE_SOURCE_DIR}/view_matching/)


target_link_libraries(RewriteLib 
    graph
    nd 
    utility 
    MatViewLib 
    # matching 
    my_utils
    SimpleMatchLib
    )
target_include_directories(RewriteLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(RewriteLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../utils/)


