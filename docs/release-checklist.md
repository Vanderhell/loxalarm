# loxalarm - Release checklist

- `CHANGELOG.md` contains a section for the tag version (for example `v0.1.0` -> `## [0.1.0]`).
- Tag, header version macros, and `CMakeLists.txt` project version match exactly.
- Public headers compile standalone (CI “header self-contained” job).
- Configure/build/test:
  - `cmake -S . -B build`
  - `cmake --build build --config Release`
  - `ctest --test-dir build --output-on-failure -C Release`
- Sanitizers (where supported by toolchain):
  - `cmake -S . -B build-sanitize -DLOXALARM_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`
  - `cmake --build build-sanitize`
  - `ctest --test-dir build-sanitize --output-on-failure`
- Examples build (if enabled): `cmake --build build --config Release`
- Install/package smoke:
  - `cmake --install build --prefix dist`
  - Consumer configure/build with `find_package(loxalarm CONFIG REQUIRED)` and `loxalarm::loxalarm`.
  - C++ consumer configure/build if the header is advertised as C++-clean.
