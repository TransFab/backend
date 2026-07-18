cmake_minimum_required(VERSION 3.25)
project(transfab-backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(functions_framework_cpp REQUIRED)
# GCP client libraries — add the ones you use (must match vcpkg.json features):
find_package(google_cloud_cpp_storage REQUIRED)
find_package(google_cloud_cpp_pubsub REQUIRED)

add_executable(backend src/hello_world.cc)
target_link_libraries(backend
    PRIVATE
        functions-framework-cpp::framework
        google-cloud-cpp::storage
        google-cloud-cpp::pubsub
)
