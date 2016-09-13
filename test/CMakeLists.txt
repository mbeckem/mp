set(SOURCES
    main.cpp
    array_2d.cpp
    parser.cpp
    tracing_data.cpp
    ground_truth.cpp
    signal_data.cpp
    serialization.cpp
    following_detection.cpp
    following_graph.cpp
    metrics.cpp
)

add_executable(${PROJECT_NAME}-test ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
