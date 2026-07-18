// TransFab backend endpoint: accepts an image, returns a mesh.
//
// Request (POST, application/json):
//   {
//     "image": "<base64-encoded PNG or JPEG bytes>",
//     "output_format": "ply" | "obj"     // optional, default "ply"
//   }
//
// Response (200, application/json):
//   {
//     "mesh": "<base64-encoded mesh file>",
//     "format": "ply" | "obj",
//     "vertex_count": <N>,
//     "face_count": <M>
//   }
//
// Errors return { "error": "<message>" } with a 4xx status.

#include <google/cloud/functions/framework.h>
#include <absl/strings/escaping.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace gcf = ::google::cloud::functions;

namespace {

constexpr int kBadRequest = 400;
constexpr int kMethodNotAllowed = 405;

enum class ImageFormat { kPng, kJpeg };

struct Mesh {
  std::vector<std::array<float, 3>> vertices;
  // Triangle faces as 0-based indices into `vertices`.
  std::vector<std::array<std::uint32_t, 3>> faces;
};

std::optional<ImageFormat> DetectImageFormat(std::string const& bytes) {
  static constexpr std::array<unsigned char, 8> kPngMagic = {
      0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  auto const* data = reinterpret_cast<unsigned char const*>(bytes.data());
  if (bytes.size() >= kPngMagic.size() &&
      std::equal(kPngMagic.begin(), kPngMagic.end(), data)) {
    return ImageFormat::kPng;
  }
  // JPEG: SOI marker 0xFFD8 followed by another marker byte 0xFF.
  if (bytes.size() >= 3 && data[0] == 0xFF && data[1] == 0xD8 &&
      data[2] == 0xFF) {
    return ImageFormat::kJpeg;
  }
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// TODO(processing): the image -> mesh pipeline goes here.
//
// `image` holds the raw decoded PNG or JPEG bytes (exactly as they would
// appear in a .png/.jpg file on disk) and `format` says which one was
// detected. Decode the pixels, run the reconstruction, and return the
// resulting geometry as a Mesh.
//
// The placeholder below returns a unit square (two triangles) so the
// endpoint can be exercised end to end before the pipeline exists.
// ---------------------------------------------------------------------------
Mesh ProcessImageToMesh(std::string const& /*image*/, ImageFormat /*format*/) {
  Mesh mesh;
  mesh.vertices = {
      {0.0F, 0.0F, 0.0F},
      {1.0F, 0.0F, 0.0F},
      {1.0F, 1.0F, 0.0F},
      {0.0F, 1.0F, 0.0F},
  };
  mesh.faces = {{0, 1, 2}, {0, 2, 3}};
  return mesh;
}

std::string SerializeToPly(Mesh const& mesh) {
  std::ostringstream out;
  out << "ply\n"
      << "format ascii 1.0\n"
      << "element vertex " << mesh.vertices.size() << "\n"
      << "property float x\nproperty float y\nproperty float z\n"
      << "element face " << mesh.faces.size() << "\n"
      << "property list uchar uint vertex_indices\n"
      << "end_header\n";
  for (auto const& v : mesh.vertices) {
    out << v[0] << ' ' << v[1] << ' ' << v[2] << '\n';
  }
  for (auto const& f : mesh.faces) {
    out << "3 " << f[0] << ' ' << f[1] << ' ' << f[2] << '\n';
  }
  return out.str();
}

std::string SerializeToObj(Mesh const& mesh) {
  std::ostringstream out;
  for (auto const& v : mesh.vertices) {
    out << "v " << v[0] << ' ' << v[1] << ' ' << v[2] << '\n';
  }
  // OBJ face indices are 1-based.
  for (auto const& f : mesh.faces) {
    out << "f " << f[0] + 1 << ' ' << f[1] + 1 << ' ' << f[2] + 1 << '\n';
  }
  return out.str();
}

gcf::HttpResponse JsonError(int status, std::string message) {
  nlohmann::json body{{"error", std::move(message)}};
  return gcf::HttpResponse{}
      .set_result(status)
      .set_header("Content-Type", "application/json")
      .set_payload(body.dump());
}

gcf::HttpResponse ImageToMesh(gcf::HttpRequest const& request) {
  if (request.verb() != "POST") {
    return JsonError(kMethodNotAllowed,
                     "use POST with a JSON body");
  }

  auto const body = nlohmann::json::parse(request.payload(), nullptr,
                                          /*allow_exceptions=*/false);
  if (body.is_discarded() || !body.is_object()) {
    return JsonError(kBadRequest,
                     "request body must be a JSON object");
  }

  auto const image_field = body.find("image");
  if (image_field == body.end() || !image_field->is_string()) {
    return JsonError(kBadRequest,
                     "missing required string field \"image\"");
  }
  std::string image;
  if (!absl::Base64Unescape(image_field->get<std::string>(), &image)) {
    return JsonError(kBadRequest,
                     "\"image\" is not valid base64");
  }
  auto const format = DetectImageFormat(image);
  if (!format) {
    return JsonError(kBadRequest,
                     "decoded image is neither PNG nor JPEG");
  }

  auto const output_format = body.value("output_format", "ply");
  if (output_format != "ply" && output_format != "obj") {
    return JsonError(kBadRequest,
                     "\"output_format\" must be \"ply\" or \"obj\"");
  }

  auto const mesh = ProcessImageToMesh(image, *format);
  auto serialized =
      output_format == "ply" ? SerializeToPly(mesh) : SerializeToObj(mesh);

  nlohmann::json response{
      {"mesh", absl::Base64Escape(serialized)},
      {"format", output_format},
      {"vertex_count", mesh.vertices.size()},
      {"face_count", mesh.faces.size()},
  };
  return gcf::HttpResponse{}
      .set_header("Content-Type", "application/json")
      .set_payload(response.dump());
}

}  // namespace

// Entry point looked up by the GCP buildpacks at deploy time
// (GOOGLE_FUNCTION_TARGET=image_to_mesh) and by the local server in
// local_run.cc. Deliberately no main() here: the buildpack generates its own
// and links it against this translation unit.
gcf::Function image_to_mesh() { return gcf::MakeFunction(ImageToMesh); }
