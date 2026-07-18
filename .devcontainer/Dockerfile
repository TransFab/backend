# Development environment for a GCP HTTP endpoint written in C++.
#
# Stack:
#   - functions-framework-cpp : Google's Functions Framework for C++ (GA).
#     C++ is not an official Cloud Run functions runtime, so C++ endpoints
#     are deployed to Cloud Run as containers built with this framework.
#   - google-cloud-cpp        : official GCP C++ client libraries.
#   - vcpkg                   : dependency manager (preinstalled in base image).
#   - gcloud CLI + pack       : deployment tooling.
#
# Base image ships gcc/clang, gdb, cmake, and vcpkg (VCPKG_ROOT=/usr/local/vcpkg).
FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

# Build tools not in the base image, plus prerequisites for the gcloud repo.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends \
        ninja-build \
        pkg-config \
        apt-transport-https \
        ca-certificates \
        gnupg \
        curl \
    && rm -rf /var/lib/apt/lists/*

# Google Cloud CLI (gcloud). Auth is done at runtime with `gcloud auth login`;
# the devcontainer mounts a volume over ~/.config/gcloud so it persists.
RUN curl -fsSL https://packages.cloud.google.com/apt/doc/apt-key.gpg \
        | gpg --dearmor -o /usr/share/keyrings/cloud.google.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] https://packages.cloud.google.com/apt cloud-sdk main" \
        > /etc/apt/sources.list.d/google-cloud-sdk.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends google-cloud-cli \
    && rm -rf /var/lib/apt/lists/*

# pack CLI (Cloud Native Buildpacks) — builds the deployable Cloud Run image
# locally. Requires a Docker daemon (see docker-outside-of-docker feature in
# devcontainer.json). Alternatively, `gcloud run deploy --source .` builds
# remotely on Cloud Build and needs no local Docker at all.
ARG PACK_VERSION=v0.36.4
RUN curl -fsSL "https://github.com/buildpacks/pack/releases/download/${PACK_VERSION}/pack-${PACK_VERSION}-linux.tgz" \
        | tar -C /usr/local/bin -xz pack

# Shared vcpkg binary cache so packages built during the image build are
# reused by manifest-mode (vcpkg.json) builds run later as the `vscode` user.
# devcontainer.json mounts a named volume here, so it also survives rebuilds.
ENV VCPKG_DEFAULT_BINARY_CACHE=/opt/vcpkg-cache
RUN mkdir -p /opt/vcpkg-cache && chmod -R 0777 /opt/vcpkg-cache

# Pre-build the heavy dependencies into the binary cache. This makes the
# image build slow (30-60 min the first time) but the first project configure
# fast. Set --build-arg PREWARM_VCPKG=0 to skip and build on first use instead.
ARG PREWARM_VCPKG=1
RUN if [ "$PREWARM_VCPKG" = "1" ]; then \
        "${VCPKG_ROOT}/vcpkg" install \
            functions-framework-cpp \
            "google-cloud-cpp[core,storage,pubsub]" \
            --clean-after-build \
        && chmod -R 0777 /opt/vcpkg-cache; \
    fi
