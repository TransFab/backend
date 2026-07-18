cmake_minimum_required(VERSION 3.25)
project(transfab-backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(functions_framework_cpp REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# GCP client libraries — add the ones you use (must match vcpkg.json features):
find_package(google_cloud_cpp_storage REQUIRED)
find_package(google_cloud_cpp_pubsub REQUIRED)

# The GCP buildpack requires a library with EXACTLY this name: it links the
# main() it generates at deploy time against this target. Do not rename it,
# and do not put a main() in its sources.
add_library(functions_framework_cpp_function src/image_to_mesh.cc)
target_link_libraries(functions_framework_cpp_function
    PUBLIC
        functions-framework-cpp::framework
        absl::strings
        nlohmann_json::nlohmann_json
        google-cloud-cpp::storage
        google-cloud-cpp::pubsub
)

# Local development server (not used by Cloud Run deployments).
add_executable(backend src/local_run.cc)
target_link_libraries(backend PRIVATE functions_framework_cpp_function)
