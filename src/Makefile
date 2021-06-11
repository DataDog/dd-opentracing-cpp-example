# the example tracer command line tool
tracer_example: tracer_example.cpp include/ghc/filesystem.hpp include/picosha2.h
	$(CXX) -o $@ -Wall -Wextra -std=c++14 -I ./include $< -ldd_opentracing -lopentracing

# a header-only C++11 library that backports C++17's std::filesystem
include/ghc/filesystem.hpp:
	mkdir -p include/ghc
	wget -O$@ https://github.com/gulrak/filesystem/releases/download/v1.5.6/filesystem.hpp

# a header-only C++98 library that implements the SHA-256 variant of the SHA-2
# hash function: <https://en.wikipedia.org/wiki/SHA-2>.
# Commit b699e6c900be6e00152db5a3d123c1db42ea13d0 was the most recent version
# of the header as of this writing (and contains fixes not present in the
# v1.0.0 release).
include/picosha2.h:
	wget -O$@ https://raw.githubusercontent.com/okdshin/PicoSHA2/b699e6c900be6e00152db5a3d123c1db42ea13d0/picosha2.h

.PHONY: clean
clean:
	rm -f include/ghc/filesystem.hpp include/picosha2.h tracer_example
