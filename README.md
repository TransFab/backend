# TransFab Backend — GCP C++ Endpoint

A C++ HTTP endpoint built with Google's [Functions Framework for C++](https://github.com/GoogleCloudPlatform/functions-framework-cpp)
and the [google-cloud-cpp](https://github.com/googleapis/google-cloud-cpp) client
libraries, deployed to **Cloud Run**.

> Note: C++ is not an official Cloud Run functions (formerly Cloud Functions)
> runtime. The Functions Framework for C++ is Google's GA-supported path for
> C++ serverless endpoints: same function signature, deployed as a Cloud Run
> container.

## Develop

Open this folder in VS Code and choose **Reopen in Container**. The first image
build pre-compiles the framework and GCP libraries via vcpkg (30–60 min once;
cached afterward in the `transfab-vcpkg-cache` volume).

Build and run locally:

```sh
cmake --preset default
cmake --build --preset default
./build/backend --port 8080
curl localhost:8080
```

## Authenticate

```sh
gcloud auth login
gcloud config set project YOUR_PROJECT_ID
gcloud auth application-default login   # for google-cloud-cpp calls during local runs
```

Credentials persist across container rebuilds (mounted volume).

## Deploy to Cloud Run

Remote build (no local Docker needed):

```sh
gcloud run deploy transfab-backend --source . --region us-central1 --allow-unauthenticated
```

Or build locally with buildpacks (uses the host Docker daemon):

```sh
pack build --builder gcr.io/buildpacks/builder:latest \
    --env GOOGLE_FUNCTION_TARGET=HelloWorld \
    gcr.io/YOUR_PROJECT_ID/transfab-backend
```

## Adding GCP services

Add the feature to `vcpkg.json` (e.g. `"spanner"`, `"bigquery"`), the matching
`find_package`/`target_link_libraries` entries to `CMakeLists.txt`, and
reconfigure. Available features: <https://vcpkg.io/en/package/google-cloud-cpp>
