# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.17

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/fexolm/Documents/git/pefa

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/fexolm/Documents/git/pefa/build

# Include any dependencies generated for this target.
include tests/CMakeFiles/test_filter_kernel.dir/depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/test_filter_kernel.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/test_filter_kernel.dir/flags.make

tests/CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.o: tests/CMakeFiles/test_filter_kernel.dir/flags.make
tests/CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.o: ../tests/test_filter_kernel.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/fexolm/Documents/git/pefa/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object tests/CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.o"
	cd /home/fexolm/Documents/git/pefa/build/tests && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.o -c /home/fexolm/Documents/git/pefa/tests/test_filter_kernel.cpp

tests/CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.i"
	cd /home/fexolm/Documents/git/pefa/build/tests && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/fexolm/Documents/git/pefa/tests/test_filter_kernel.cpp > CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.i

tests/CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.s"
	cd /home/fexolm/Documents/git/pefa/build/tests && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/fexolm/Documents/git/pefa/tests/test_filter_kernel.cpp -o CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.s

# Object files for target test_filter_kernel
test_filter_kernel_OBJECTS = \
"CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.o"

# External object files for target test_filter_kernel
test_filter_kernel_EXTERNAL_OBJECTS =

tests/test_filter_kernel: tests/CMakeFiles/test_filter_kernel.dir/test_filter_kernel.cpp.o
tests/test_filter_kernel: tests/CMakeFiles/test_filter_kernel.dir/build.make
tests/test_filter_kernel: /home/fexolm/miniconda3/envs/pefa-dev/lib/libgtest.a
tests/test_filter_kernel: /home/fexolm/miniconda3/envs/pefa-dev/lib/libgtest_main.a
tests/test_filter_kernel: libpefa.so
tests/test_filter_kernel: tests/libarrow_testing.a
tests/test_filter_kernel: /home/fexolm/miniconda3/envs/pefa-dev/lib/libLLVM-10.so
tests/test_filter_kernel: /home/fexolm/miniconda3/envs/pefa-dev/lib/libarrow.so.17.1.0
tests/test_filter_kernel: /home/fexolm/miniconda3/envs/pefa-dev/lib/libgtest.a
tests/test_filter_kernel: /home/fexolm/miniconda3/envs/pefa-dev/lib/libgtest_main.a
tests/test_filter_kernel: tests/CMakeFiles/test_filter_kernel.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/fexolm/Documents/git/pefa/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable test_filter_kernel"
	cd /home/fexolm/Documents/git/pefa/build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_filter_kernel.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/test_filter_kernel.dir/build: tests/test_filter_kernel

.PHONY : tests/CMakeFiles/test_filter_kernel.dir/build

tests/CMakeFiles/test_filter_kernel.dir/clean:
	cd /home/fexolm/Documents/git/pefa/build/tests && $(CMAKE_COMMAND) -P CMakeFiles/test_filter_kernel.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/test_filter_kernel.dir/clean

tests/CMakeFiles/test_filter_kernel.dir/depend:
	cd /home/fexolm/Documents/git/pefa/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fexolm/Documents/git/pefa /home/fexolm/Documents/git/pefa/tests /home/fexolm/Documents/git/pefa/build /home/fexolm/Documents/git/pefa/build/tests /home/fexolm/Documents/git/pefa/build/tests/CMakeFiles/test_filter_kernel.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/CMakeFiles/test_filter_kernel.dir/depend

