// Local development server. Cloud Run deployments do not use this file: the
// GCP buildpack generates its own main() and links it against the function
// library (see CMakeLists.txt and GOOGLE_FUNCTION_TARGET in the README).

#include <google/cloud/functions/framework.h>

namespace gcf = ::google::cloud::functions;

gcf::Function image_to_mesh();

int main(int argc, char* argv[]) {
  return gcf::Run(argc, argv, image_to_mesh());
}
