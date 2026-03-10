set(project FullSampleStructAlignmentTests)
set(folder "RTXDI SDK Tests")

set(sources Source/AlignmentTests.cpp)

add_library(${project} OBJECT ${sources})
target_link_libraries(${project} donut_core Rtxdi)
add_dependencies(${project} FullSampleShaders)
set_target_properties(${project} PROPERTIES FOLDER ${folder})
target_include_directories(${project} PRIVATE ${FULL_SAMPLE_SHARED_INCLUDE_PATH})
