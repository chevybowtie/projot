#!/bin/bash
set -euo pipefail

# Package .deb and tar.gz from a staged install tree
# Usage: scripts/package.sh --version <version> --prefix <staging-dir> --output-dir <output-dir>

version=""
prefix=""
output_dir="."

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            version="$2"
            shift 2
            ;;
        --prefix)
            prefix="$2"
            shift 2
            ;;
        --output-dir)
            output_dir="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

if [[ -z "$version" || -z "$prefix" ]]; then
    echo "Usage: $0 --version <version> --prefix <staging-dir> [--output-dir <output-dir>]" >&2
    exit 1
fi

if [[ ! -d "$prefix" ]]; then
    echo "Error: staging directory does not exist: $prefix" >&2
    exit 1
fi

# Create DEBIAN/control directory structure for .deb
debian_dir="$prefix/DEBIAN"
mkdir -p "$debian_dir"

# Generate control file
cat > "$debian_dir/control" << EOF
Package: projot
Version: $version
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Paul Sturm <bills@paulsturm.net>
Description: Repo-centric developer notepad
 CLI tool for tracking todos, notes, and URLs inside a .projot/
 directory at a git repository root.
EOF

# Build .deb
deb_file="$output_dir/projot_${version}_amd64.deb"
dpkg-deb --build --root-owner-group "$prefix" "$deb_file"
echo "Created: $deb_file"

# Build tar.gz from the same staging tree (without DEBIAN metadata)
# We'll create a clean tarball with just the install tree (bin/, share/, etc.)
tar_file="$output_dir/projot-${version}-linux-x86_64.tar.gz"
tar -czf "$tar_file" \
    --exclude=DEBIAN \
    -C "$prefix" .
echo "Created: $tar_file"
