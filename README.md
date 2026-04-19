# CMakeFileEmbedder

## Overview

A lightweight, high-performance CMake tool for embedding files directly into executables.

This project requires **no code changes**, generates **no temporary files**, and does **not rely on any runtime API**. It integrates seamlessly into existing CMake workflows and works on both Windows and Linux.

Designed for minimal integration friction in existing CMake projects.

## Features

* High-performance embedding
* Zero code changes required
* No temporary file generation
* No runtime API or dependencies
* Simple CMake integration
* Cross-platform (Windows & Linux)

## How It Works

This tool hooks into the CMake build process and embeds files directly into the final executable at build time by generating C++ header and source files containing the file data.

Unlike traditional approaches, it does not generate intermediate source files or require manual runtime access code.

## Setup

### FetchContent (Recommended)

You can also integrate this project using CMake FetchContent:

```cmake
include(FetchContent)

FetchContent_Declare(
    CMakeFileEmbedder
    GIT_REPOSITORY https://github.com/KyfStore11k/CMakeFileEmbedder.git
    GIT_TAG master
)

FetchContent_MakeAvailable(CMakeFileEmbedder)
include("${CMakeFileEmbedder_SOURCE_DIR}/cmake/CMakeFileEmbedder.cmake")
```

Then use it normally:

```cmake
add_embedded_files(MyProject assets/icon.png assets/font.ttf)
```

### FindPackage (Installed Usage)
If you prefer installing the library and using it via `find_package`, you can build and install it manually.
#### 1. Build and Install

Clone the repository:

```bash
git clone https://github.com/KyfStore11k/CMakeFileEmbedder.git
cd CMakeFileEmbedder
```

Configure and install:

```bash
cmake -S . -B build
cmake --build build
cmake --install build
```

By default, this will install the package into your system CMake prefix (e.g., `/usr/local` on Linux or `C:\Program Files` on Windows).

You can customize the install location:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/install/path
cmake --build build
cmake --install build
```
#### 2. Using `find_package`
After installation, you can use it in your project like this:
```cmake
find_package(CMakeFileEmbedder REQUIRED CONFIG)

# add_main_executable_file(MyProject main.cpp)
add_embedded_files(MyProject
    assets/icon.png
    assets/font.ttf
)
```

### add_subdirectory (Alternative)

1. Add the project to your repository:

* `git clone https://github.com/KyfStore11k/CMakeFileEmbedder.git`

2. Include it in your `CMakeLists.txt`:

* `add_subdirectory(CMakeFileEmbedder)`

# Example

```cmake
# add_main_executable_file(MyProject main.cpp)
add_embedded_files(MyProject assets/icon.png assets/font.ttf)
```

***Disclaimer: Windows projects require an additional setup step for the executable entry point (which is the commented out line); Be sure to change MyProject to your own project name and change main.cpp to the actual file that contains your entry point.***

## Usage

Build your project as usual using CMake.
The specified files will be embedded directly into the resulting executable.

No additional code or API calls are required.

## Use Cases

* Bundling assets (images, configs, shaders, etc.)
* Creating single-file executables
* Eliminating runtime file dependencies

## Notes

* File size may increase the final executable size
* Ensure embedded files are appropriate for compile-time inclusion

## Contributing

Contributions are welcome! Feel free to open an issue or submit a pull request.

## License

This project is licensed under the MIT License. See [LICENSE](https://github.com/KyfStore11k/CMakeFileEmbedder/tree/master/LICENSE) for details.
