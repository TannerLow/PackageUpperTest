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

- imports `deps/deps.props`, which PackageUpper writes on every install. It puts each library's include folders on the include path and compiles every library source file, so files the libraries add or remove are picked up without editing the project
- links `bass.lib`, `bass_fx.lib` and `raylib.lib` (static), set in the project by hand
- copies `bass.dll`, `bass_fx.dll` and `resources/` next to the executable after each build
