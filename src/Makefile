# the example tracer command line tool
tracer_example: tracer_example.cpp include/ghc/filesystem.hpp
	$(CXX) -o $@ -Wall -Wextra -std=c++14 -I ./include $< -ldd_opentracing -lopentracing

# a header-only C++11 library that backports C++17's std::filesystem
include/ghc/filesystem.hpp:
	mkdir -p include/ghc
	wget -O$@ https://github.com/gulrak/filesystem/releases/download/v1.5.6/filesystem.hpp
