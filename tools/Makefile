CXXFLAGS ?= -O2 -Wall -Wextra
CXXFLAGS += -std=c++17 -I../src

render: render.cpp $(wildcard *.hpp) $(wildcard ../src/core/*.hpp)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f render

.PHONY: clean
