# Build

## .deb build

Build a local Debian package using the same flow as the GitHub release workflow.

```sh
cd ~/Documents/projects/projot
version=$(sed -n 's/^project(projot VERSION \([0-9.]\+\).*/\1/p' CMakeLists.txt)

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

rm -rf dist/staging
cmake --install build --prefix dist/staging/usr
bash scripts/package.sh --version "$version" --prefix dist/staging --output-dir dist

ls -lh dist/projot_*_amd64.deb dist/projot-*-linux-x86_64.tar.gz
```

The `.deb` artifact is written to:

- `dist/projot_${version}_amd64.deb`

Optional validation:

```sh
dpkg-deb -I dist/projot_${version}_amd64.deb
dpkg-deb -c dist/projot_${version}_amd64.deb | head
sudo apt install ./dist/projot_${version}_amd64.deb
projot --version
```
