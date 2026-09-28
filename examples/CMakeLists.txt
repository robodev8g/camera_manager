find_package(OpenCV 4 REQUIRED COMPONENTS core highgui videoio)

add_executable(simple-live-view simple_live_view.cpp)
target_include_directories(simple-live-view PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(simple-live-view PRIVATE ${OpenCV_LIBS})
