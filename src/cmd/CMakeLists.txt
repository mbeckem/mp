include(CMakeParseArguments)

set(COMMON_HEADERS
    ${HEADERS}

    common/binary_classifier.hpp
    common/classifier_file.hpp
    common/eval_classifier_file.hpp
    common/eval_follower_file.hpp
    common/eval_leader_file.hpp
    common/feature_file.hpp
    common/follower_file.hpp
    common/leader_file.hpp
    common/ground_truth_file.hpp
    common/parser.hpp
    common/scene_manifest.hpp
    common/util.hpp
)

set(COMMON_SOURCES
    common/parser.cpp
    common/scene_manifest.cpp
)

set(LIBS
    ${PROJECT_NAME} # the common library
    ${Boost_LIBRARIES}
)

add_library(common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
target_link_libraries(common ${LIBS})

macro(make_executable name)
    cmake_parse_arguments(MAKE_EXECUTABLE "" "" "SOURCES" ${ARGN})

    add_executable(${name} ${MAKE_EXECUTABLE_SOURCES} ${COMMON_HEADERS})
    target_link_libraries(${name} common)
    install(TARGETS ${name} DESTINATION bin)
endmacro()

make_executable(produce-features SOURCES
    produce-features/main.cpp
)

make_executable(produce-ground-truth SOURCES
    produce-ground-truth/main.cpp
)

make_executable(train-classifier SOURCES
    train-classifier/main.cpp
)
make_executable(evaluate-classifier SOURCES
    evaluate-classifier/main.cpp
)
make_executable(detect-followers SOURCES
    detect-followers/main.cpp
)
make_executable(evaluate-followers SOURCES
    evaluate-followers/main.cpp
)
make_executable(evaluate-leaders SOURCES
    evaluate-leaders/main.cpp
)
make_executable(following-graph SOURCES
    following-graph/main.cpp
)
make_executable(detect-leaders SOURCES
    detect-leaders/main.cpp
)
make_executable(dtw-path-example SOURCES
    dtw-path-example/main.cpp
)
make_executable(group-eval SOURCES
    group-eval/main.cpp
)
