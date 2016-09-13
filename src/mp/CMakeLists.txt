set(SOURCES
    parser.cpp
    tracing_data.cpp
    metrics.cpp
    feature_computation.cpp
    co_moving_detection.cpp
    following_detection.cpp
    ground_truth.cpp
    signal_data.cpp
    following_graph.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} "dlib")
target_link_libraries(${PROJECT_NAME} "pugixml")
install(TARGETS ${PROJECT_NAME} DESTINATION lib)
