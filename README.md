# TransFab Backend — GCP C++ Endpoint

A C++ HTTP endpoint built with Google's [Functions Framework for C++](https://github.com/GoogleCloudPlatform/functions-framework-cpp)
and the [google-cloud-cpp](https://github.com/googleapis/google-cloud-cpp) client
libraries, deployed to **Cloud Run**.

> **Note:** C++ is not an official Cloud Run functions (formerly Cloud
> Functions) runtime. The Functions Framework for C++ is Google's GA-supported
> path for C++ serverless endpoints: you write the same
> `HttpRequest → HttpResponse` function signature as an official runtime, and
> it deploys to Cloud Run as a container.

## Prerequisites

- VS Code with the **Dev Containers** extension, and Docker on the host.
- A GCP project with billing enabled.

## 1. Open the development environment

Open this folder in VS Code and choose **Reopen in Container**.

- The **first** image build pre-compiles `functions-framework-cpp` and
  `google-cloud-cpp` (storage, pubsub) via vcpkg — expect 30–60 minutes, once.
  The results are cached in the `transfab-vcpkg-cache` Docker volume and
  reused across container rebuilds.
- To skip the pre-compile (build dependencies lazily on first configure
  instead), set `"args": { "PREWARM_VCPKG": "0" }` under `build` in
  `.devcontainer/devcontainer.json`.

## 2. Build

From a terminal inside the container:

```sh
cmake --preset default          # configure (vcpkg resolves vcpkg.json)
cmake --build --preset default  # compile → build/backend
```

Or use the CMake Tools extension — the presets are picked up automatically.
For an optimized binary use the `release` preset instead of `default`.

## 3. Run and test locally

```sh
./build/backend --port 8080
```

In another terminal, POST an image and decode the returned mesh:

```sh
curl -s -X POST localhost:8080 \
    -H "Content-Type: application/json" \
    -d "{\"image\": \"$(base64 -w0 input.png)\", \"output_format\": \"obj\"}" \
    | jq -r .mesh | base64 -d > output.obj
```

The framework serves HTTP the same way Cloud Run will (it honors `$PORT`).

### API

`POST /` with a JSON body:

| Field | Type | Description |
|---|---|---|
| `image` | string, required | Base64-encoded PNG or JPEG bytes |
| `output_format` | string, optional | `"ply"` (default) or `"obj"` |

Success response (`200`): `{ "mesh": "<base64 mesh file>", "format": "ply"|"obj", "vertex_count": N, "face_count": M }`.
Errors return `{ "error": "<message>" }` with a `4xx` status.

The image → mesh reconstruction is not implemented yet: `ProcessImageToMesh()`
in `src/image_to_mesh.cc` is a marked placeholder that returns a unit square,
so the endpoint is fully exercisable end to end.

## 4. Authenticate with GCP

One-time setup (persists across container rebuilds via a mounted volume):

```sh
gcloud auth login
gcloud config set project YOUR_PROJECT_ID
gcloud auth application-default login   # credentials for google-cloud-cpp calls in local runs
```

If a browser can't open inside the container, run the printed URL on the host
and paste the code back.

## 5. Deploy to Cloud Run

One-time project setup:

```sh
gcloud services enable run.googleapis.com cloudbuild.googleapis.com artifactregistry.googleapis.com
```

### Option A — local buildpack build, then deploy (the working path)

> **Why not `gcloud run deploy --source .`?** Google's current Cloud Build
> universal builder no longer includes the C++ buildpack, so remote source
> deploys fail with `No buildpack groups passed detection` (verified
> 2026-07-19). The same applies to `gcr.io/buildpacks/builder:latest`
> locally. The `builder:v1` image still ships the C++ Functions Framework
> buildpack — use it until C++ support returns to the current builders.

Uses the host Docker daemon (available in the container via
docker-outside-of-docker). One-time: create an Artifact Registry repo and
configure Docker auth:

```sh
gcloud artifacts repositories create REPO --repository-format=docker --location=us-central1
gcloud auth configure-docker us-central1-docker.pkg.dev
```

Then build, push, and deploy:

```sh
pack build --builder gcr.io/buildpacks/builder:v1 \
    --env GOOGLE_FUNCTION_TARGET=image_to_mesh \
    us-central1-docker.pkg.dev/YOUR_PROJECT_ID/REPO/transfab-backend

docker push us-central1-docker.pkg.dev/YOUR_PROJECT_ID/REPO/transfab-backend

gcloud run deploy transfab-backend \
    --image us-central1-docker.pkg.dev/YOUR_PROJECT_ID/REPO/transfab-backend \
    --region us-central1 \
    --allow-unauthenticated
```

The first `pack build` compiles the whole vcpkg dependency tree inside the
builder and takes a long time; subsequent builds reuse the buildpack's cache
layers. If `pack` reports the Docker client API version is too old, install
the latest release from <https://github.com/buildpacks/pack/releases>. A
transient `network is unreachable` during `docker push` is retryable —
already-pushed layers are skipped.

The deploy prints the service URL. Test it with the same curl call as in
section 3, pointing at the URL:

```sh
curl -s -X POST https://transfab-backend-<hash>-uc.a.run.app \
    -H "Content-Type: application/json" \
    -d "{\"image\": \"$(base64 -w0 input.png)\", \"output_format\": \"obj\"}" \
    | jq -r .mesh | base64 -d > output.obj
```

Drop `--allow-unauthenticated` for a private endpoint (callers then need an
identity token: `curl -H "Authorization: Bearer $(gcloud auth print-identity-token)" <url>`).

### Option B — remote build (currently broken for C++)

```sh
gcloud run deploy transfab-backend \
    --source . \
    --set-build-env-vars GOOGLE_FUNCTION_TARGET=image_to_mesh \
    --region us-central1 \
    --allow-unauthenticated
```

This is the intended zero-local-Docker path: Cloud Build applies Google's
buildpacks, builds the image remotely, and deploys it. As of 2026-07-19 it
fails during detection (see the note in Option A) because the remote
universal builder has no C++ buildpack. Kept here in case Google restores
C++ support. Source uploads honor `.gcloudignore` — keep `build/` listed
there (it is multiple GB).

### How the buildpack finds the function

`GOOGLE_FUNCTION_TARGET` names a factory function returning `gcf::Function`
(`image_to_mesh()` in `src/image_to_mesh.cc`). The buildpack generates its own
`main()` at deploy time and links it against the CMake library target
`functions_framework_cpp_function` — that target name is a buildpack contract
(see `CMakeLists.txt`), and its sources must not define a `main()`. The local
dev server's `main()` lives separately in `src/local_run.cc` and is only built
for the `backend` executable.

## Adding more GCP services

1. Add the feature to `vcpkg.json` (e.g. `"spanner"`, `"bigquery"`).
2. Add the matching `find_package(google_cloud_cpp_<name>)` and
   `google-cloud-cpp::<name>` link entry in `CMakeLists.txt`.
3. Reconfigure: `cmake --preset default`.

Available features: <https://vcpkg.io/en/package/google-cloud-cpp>
