# Agent instructions: nnimage

## Project overview
A all-in-one CLI disk image management built in C++. Primarily for Linux and macOS now, with portability towards Windows and BSD being a future goal

## Development commands
- Check CMakeLists.txt for all current dependencies
- IMPORTANT: do NOT install libkrun from distro repos. Try to use scripts/build_libkrun where possible
- Do not build with libguestfs unless specifically asked to
- To build: run `mkdir build && cd build && cmake .. -DNNIMAGE_ENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DNNIMAGE_ENABLED_BACKENDS="krun;xorriso;loopback" && cmake --build . -j`
- After a successful build, always run the full test suite via `ctest`

## Code style
- Use idiomatic C++23, making full use of STL. Reference .clang-format for code style guide. If a clang-format off code block is encountered, reference the style found in the block and remain consistent with it
- To communicate function results, always use the custom Result class found in source/include/Error.h rather than std::expected. Use Error objects to communicate essentially all error conditions. Use standardized error format strings as found in include/sys/ErrorCode.h and follow all conventions found there. Only custom-format error messages in modules where that is the style.
- Only use iostreams for rudimentary file I/O. For anything complex, use either the MemoryMapped class, the TextReader class, or C streams. This is because of poor error communication found in iostreams.
- Do not duplicate code if not necessary. Attempt to use templated base classes, inheritance, and containers to avoid duplicate code. Please look at include/Image.h and it's sub-includes for examples on how to structure complex containers.
- When making a hash table addressed by a string, using StringHash for the hash comparator. This is to avoid copies when using a string_view to look up a hash table entry

## Important notes
- This project is very imcomplete. Do not over-architect; if a problem requires extensive plumbing that has not been added yet, do not attempt to over-engineer. Simply report the problem.
- Always add test cases for essentially all new functionality.
