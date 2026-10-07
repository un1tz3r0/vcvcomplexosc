RACK_DIR ?= ../..

FLAGS += -Isrc
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk

# The core uses C++17; the later -std wins over plugin.mk's c++11.
CXXFLAGS += -std=c++17
