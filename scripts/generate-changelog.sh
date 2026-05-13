#!/bin/bash
# Generate CHANGELOG.md using git-cliff

set -e

if ! command -v git-cliff &> /dev/null; then
    echo "Error: git-cliff is not installed"
    echo "Install it with: cargo install git-cliff"
    exit 1
fi

echo "Generating CHANGELOG.md..."
git-cliff --config cliff.toml --output CHANGELOG.md
echo "✓ CHANGELOG.md generated"
echo ""
echo "Preview (last 30 lines):"
tail -30 CHANGELOG.md
