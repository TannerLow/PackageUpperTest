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

Open `PackageUpperTest.sln` and build `x64`. The project imports `deps/deps.props`, which PackageUpper writes on every install. It sets everything the libraries need:

- each library's include folders on the include path, and every library source file compiled
- `bass.lib`, `bass_fx.lib`, `raylib.lib` and `winmm.lib` linked, as the libraries' `packageupper.txt` files ask
- `bass.dll` and `bass_fx.dll` copied next to the executable after each build

The project itself only adds a post-build step that copies its own `resources/` next to the executable. Win32 builds don't link, because the raylib that comes with O2Gui is x64 only.
