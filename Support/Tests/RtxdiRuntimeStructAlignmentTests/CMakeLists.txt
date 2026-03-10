set(project RtxdiRuntimeStructAlignmentTests)
set(folder "RTXDI SDK Tests")

set(sources Source/AlignmentTests.cpp)

add_library(${project} OBJECT ${sources})
target_link_libraries(${project} Rtxdi)
set_target_properties(${project} PROPERTIES FOLDER ${folder})