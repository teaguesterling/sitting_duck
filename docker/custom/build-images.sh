#!/bin/bash

# Build custom Docker images locally for testing
set -e

# Configuration
REGISTRY="ghcr.io"
REPO="teaguesterling/duckdb_ast/duckdb-extension"
TAG="latest"
VCPKG_URL="https://github.com/microsoft/vcpkg.git"
VCPKG_COMMIT="ce613c41372b23b1f51333815feb3edd87ef8a8b"
EXTRA_TOOLCHAINS=";rust;"

# Architectures to build
ARCHS=("linux_amd64" "linux_arm64" "linux_amd64_musl")

echo "Building custom Docker images..."
echo "Registry: $REGISTRY"
echo "Repository: $REPO"
echo "Tag: $TAG"
echo ""

for arch in "${ARCHS[@]}"; do
    echo "Building $arch..."
    
    docker build \
        -f "Dockerfile.$arch" \
        --build-arg "vcpkg_url=$VCPKG_URL" \
        --build-arg "vcpkg_commit=$VCPKG_COMMIT" \
        --build-arg "extra_toolchains=$EXTRA_TOOLCHAINS" \
        -t "$REGISTRY/$REPO:$TAG-$arch" \
        ../..
    
    echo "✅ Built $REGISTRY/$REPO:$TAG-$arch"
    echo ""
done

echo "All images built successfully!"
echo ""
echo "To push to registry:"
for arch in "${ARCHS[@]}"; do
    echo "  docker push $REGISTRY/$REPO:$TAG-$arch"
done
echo ""
echo "To test locally:"
echo "  docker run --rm -it $REGISTRY/$REPO:$TAG-linux_amd64 bash"