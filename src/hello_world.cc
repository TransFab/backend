#include <google/cloud/functions/framework.h>

namespace gcf = ::google::cloud::functions;

// The same signature Cloud Run functions use in official runtimes. The
// buildpack (or the local main() below) wires it to an HTTP server that
// listens on $PORT, as Cloud Run expects.
gcf::HttpResponse HelloWorld(gcf::HttpRequest const& /*request*/) {
  return gcf::HttpResponse{}
      .set_header("Content-Type", "text/plain")
      .set_payload("Hello from the TransFab backend\n");
}

int main(int argc, char* argv[]) {
  return gcf::Run(argc, argv, gcf::MakeFunction(HelloWorld));
}
