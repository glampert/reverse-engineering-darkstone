
# Reverse engineering Darkstone game file formats

This repository contains a couple tools and code to open MTF archives
and view O3D models from the game [Darkstone: Evil Reigns](https://en.wikipedia.org/wiki/Darkstone).

A partial description of the MTF format can be found [here](http://wiki.xentax.com/index.php?title=Darkstone).

Currently there's full support for decompressing MTF game archives into
normal files in the file system. No support for rebuilding the archives
is implemented, but it shouldn't be very had to do the inverse process
and pack files back into an MTF...

There are two tools in the project:

- `mtf_unpacker`: A very simple command line tool to decompress an MTF into normal files.

- `o3d_viewer`: A simple OpenGL-based viewer for the O3D models used by the game.
Once you unpack the MTF archives, you can use this viewer to render the static models.

## Directory structure / dependencies

The `src/` directory contains the source code for the tools and necessary dependencies,
including third-party code. Files of interest are: `mtf.h/.c` and `o3d.h/.c`.

The `shaders/` directory contains the GLSL shaders used by `o3d_viewer`.

The provided `Makefile` was only tested on Mac OSX, but it should work on most
Unix-like systems without changes or minimal changes.

The only external dependency not included in the project is [**GLFW**](http://www.glfw.org/),
which is used by the `o3d_viewer` to manage window and context creation in a portable way.
If you only care about the MTF unpacker, then this dependency is not required, since you can
compile it in isolation. The viewer requires GLFW to be properly installed and visible
in the default include path. [GL3W](https://github.com/skaslev/gl3w) is used to load the OpenGL Library
and [STB Image](https://github.com/nothings/stb) for image loading from file, but those are included in the project.

## Porting

As mentioned previously, the `o3d_viewer` depends on GLFW, so the library must be installed
in your system. Apart from that, it shouldn't have any other system specific dependencies.

The `mtf_unpacker`, on the other hand, uses the `stat/mkdir` APIs to create the file paths
for extraction. These functions exist on any Unix-based system, but MS Windows defines them
with different names and in different header files, so if you're interested in porting this,
make sure to fix that detail first.

As mentioned above, the project was only built and tested on Mac OSX.
The viewer also requires OpenGL 3.2 or above to run properly (shouldn't be too rare nowadays).

## Building

Just navigate to the project's directory and run either `make viewer` or `make unpacker`
to build the `o3d_viewer` and `mtf_unpacker`, respectively.

It will perform an "in-source" build, outputting the `.o` files in the `src/` dir,
but the executables are outputted in the root directory of the project.

Run `make clean` to delete all outputs from a previous build.

## Running the tools

The `mtf_unpacker` takes two parameters, the source MTF file to extract and a path
where to dump all the contents. Original paths are preserved in the output. Example:

> `$ ./mtf_unpacker DATA.MTF dump/`

The `o3d_viewer` takes the name of the O3D model file to view and optionally a texture map
to apply. If the texture filename is omitted, it applies a default checkerboard texture. Example:

> `$ ./o3d_viewer KNIGHT.O3D K0015_KNIGHT.TGA`

The viewer doesn't provide much user interaction, but you can right click the window
to cycle the available render modes (textured, wireframe, color-only, etc) and left click
and hold then drag to rotate the model. Mouse wheel zooms in/out.

## License

This project's source code is released under the [MIT License](http://opensource.org/licenses/MIT).

## Eye candy

![O3D Viewer](https://raw.githubusercontent.com/glampert/reverse-engineering-darkstone/master/o3d-viewer.png "O3D Viewer samples")

---

Have fun!

