# Custom Docker Images for DuckDB AST Extension

This directory contains custom Docker images that extend the standard DuckDB extension build environment with Node.js and tree-sitter CLI support.

## Overview

The custom Docker images are based on the same base images used by DuckDB's extension-ci-tools but include:

- **Node.js v22.11.0 LTS** - JavaScript runtime
- **npm** - Node.js package manager
- **tree-sitter-cli** - Tree-sitter grammar compilation tool
- **CMake 4.1.2** - Build system generator

## Architecture Support

- `linux_amd64` - Based on `manylinux_2_28_x86_64`
- `linux_arm64` - Based on `manylinux_2_28_aarch64` 
- `linux_amd64_musl` - Based on `alpine:3`

## Usage

### Automatic Build via GitHub Actions

The custom images are automatically built and pushed to GitHub Container Registry when:
- Changes are made to files in `docker/custom/`
- The build workflow is manually triggered
- Changes are pushed to main branch

### Manual Build and Push

```bash
# Build locally for testing
docker build -f docker/custom/Dockerfile.linux_amd64 \
  --build-arg vcpkg_url=https://github.com/microsoft/vcpkg.git \
  --build-arg vcpkg_commit=ce613c41372b23b1f51333815feb3edd87ef8a8b \
  --build-arg extra_toolchains=";rust;" \
  -t ghcr.io/teaguesterling/duckdb_ast/duckdb-extension:latest-linux_amd64 .

# Push to registry (requires authentication)
docker push ghcr.io/teaguesterling/duckdb_ast/duckdb-extension:latest-linux_amd64
```

### Using in CI

The custom distribution workflow (`.github/workflows/custom-distribution.yml`) uses these pre-built images instead of building Docker images on-demand, which:

1. **Reduces CI build time** - No need to install Node.js during CI
2. **Prevents disk space issues** - Pre-built images are smaller than installing everything during build
3. **Ensures consistency** - Same Node.js/tree-sitter versions across all builds

## Image Contents

All images include the standard DuckDB extension build environment plus:

- CMake 4.1.2
- Ninja build system
- ccache for compilation caching
- AWS CLI for artifact uploads
- Git configuration for mounted volumes
- VCPKG package manager
- **Node.js v22 LTS and npm** (added by us)
- **tree-sitter CLI** (added by us)

### Conditional Toolchains

The images still support the standard extension-ci-tools toolchains:
- `rust` - Rust compiler and cargo
- `parser_tools` - bison and flex (redundant with tree-sitter but available)
- `fortran` - Fortran compiler
- `go` - Go compiler

## Container Registry

Images are stored in GitHub Container Registry:
- Registry: `ghcr.io`
- Repository: `teaguesterling/duckdb_ast/duckdb-extension`
- Tags: `{branch}-{architecture}` (e.g., `main-linux_amd64`)

## Troubleshooting

### Authentication Issues
Ensure you're logged in to GHCR:
```bash
echo $GITHUB_TOKEN | docker login ghcr.io -u $GITHUB_USERNAME --password-stdin
```

### Image Not Found
Trigger the build workflow manually or check that the images were built successfully in GitHub Actions.

### Node.js/npm Issues
The images use Node.js v22.11.0 LTS (Active LTS). If you need a different version, modify the Dockerfiles and rebuild.