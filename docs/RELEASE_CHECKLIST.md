# ComTrace Release Checklist

Current project version: **0.2.0** (from `project(ComTrace VERSION 0.2.0)` in `CMakeLists.txt`).

This checklist is a release gate, not evidence that an unchecked item has passed. Keep the release on `dev` until every required validation item has evidence.

## Automated gates

- [ ] Latest `dev` CI run passes Release configure/build and CTest.
- [ ] Latest `dev` CI run passes Debug build and CTest.
- [ ] The greater-than-4-GiB archive stress workload passes.
- [ ] Release package contents and SHA-256 checksum generation pass.
- [ ] Real COM ETL capture and ETL-to-CTRA round-trip validation pass.
- [ ] CTRA strict inspection and CRC-32 validation pass.
- [ ] The CI OLE fixture passes its headless-safe IDataObject and drop-target registration checks.

## Manual Windows validation

- [ ] Windows 10 x64: Release and Debug build and test.
- [ ] Windows 10 x64: real COM capture and ETL-to-CTRA round trip.
- [ ] Windows 10 x64: interactive clipboard and drag-and-drop OLE lifecycle validation.
- [ ] Windows 11 x64: Release and Debug build and test.
- [ ] Windows 11 x64: real COM capture and ETL-to-CTRA round trip.
- [ ] Windows 11 x64: interactive clipboard and drag-and-drop OLE lifecycle validation.
- [ ] Repeat capture and archive validation runs; confirm ETW sessions stop and temporary files do not accumulate.
- [ ] Investigate native heap and handle behavior with Windows Application Verifier or an equivalent diagnostic tool.
- [ ] Review every validation JSON summary and retain it with the release evidence.

The supported release target is x64. x86 and ARM64 are not release targets until dedicated builds, tests, and real-trace validation are added and pass.

## Release preparation

- [ ] Review README and all user-facing documentation for current behavior and limitations.
- [ ] Confirm `CMakeLists.txt`, `ComTrace.exe --version`, and release tag use the same version.
- [ ] Write release notes describing supported features, compatibility, known limitations, and validation evidence.
- [ ] Review the ZIP contents and standalone executables from the release package.
- [ ] Verify SHA-256 checksums after downloading the package.
- [ ] Obtain explicit approval before creating a `v0.2.0` tag or publishing a GitHub Release.

The release workflow in `.github/workflows/release.yml` builds and tests the tagged source, packages the executables and supporting files, and publishes the generated ZIP and checksums. Do not create a tag merely to test the workflow; the CI workflow already checks package creation and contents without publishing a release.

## Release notes template

### Added
- 

### Changed
- 

### Fixed
- 

### Compatibility and requirements
- Windows 10/11 x64
- CTRA v1 archives remain readable; new archives use CTRA v2 with CRC-32.

### Validation
- CI run:
- Windows 10 validation evidence:
- Windows 11 validation evidence:
- Interactive OLE validation evidence:

### Known limitations
- 
