# PackageUpperTest

A test project for [PackageUpper](https://github.com/TannerLow/PackageUpper). It gets O2Jam and O2Gui through PackageUpper, and `main.cpp` uses both: a window with Play and Stop buttons for a song.

## Layout

- `PackageUpperTest/packageupper.txt`: the libraries this project depends on, each pinned to a commit
- `PackageUpperTest/deps/`: what PackageUpper installed. Committed, and not edited by hand unless the dependency is marked `local`
- `PackageUpperTest/resources/example_song.mp3`: copied by hand from O2Jam's resources

## Updating the libraries

Change a commit in `PackageUpperTest/packageupper.txt`, then run:

```bash
python path/to/PackageUpper/packageupper.py install --project PackageUpperTest
```

Commit the changes to `deps/` along with `packageupper.txt`.

## Building

Open `PackageUpperTest.sln` and build `x64`. The project:

- compiles the `.cpp` files under `deps/*/src/`, listed one by one because Visual Studio doesn't support wildcards in `.vcxproj` items. When an install adds or removes library files, update the list in `PackageUpperTest.vcxproj` and `PackageUpperTest.vcxproj.filters`
- puts each dependency's `include/` and its externals' `include/` on the include path
- links `bass.lib`, `bass_fx.lib` and `raylib.lib` (static)
- copies `bass.dll`, `bass_fx.dll` and `resources/` next to the executable after each build
