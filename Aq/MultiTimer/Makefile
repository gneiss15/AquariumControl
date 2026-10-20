# Makefile, Version: 1.18

CXX ?= g++

RasPiModel := $(shell \
  test -r /proc/device-tree/model && \
  tr -d '\0' < /proc/device-tree/model 2>/dev/null)

DetectedSystem := dummy
ifneq ($(findstring Raspberry Pi,$(RasPiModel)),)
  DetectedSystem := raspi
endif

TARGET_SYSTEM ?= $(DetectedSystem)
PROGRAM := MultiTimer
BUILD_DIR := generated/build/$(TARGET_SYSTEM)
VERSION_HEADER := Version.inc.h

GnLibRoot ?= ../Libs
GnBasicLibDir := $(GnLibRoot)/GnBasicLib

INCLUDE_DIRS ?= include
INCLUDE_DIRS += $(GnBasicLibDir)

CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS))

ifeq ($(DetectedSystem),raspi)
  CPPFLAGS += -DGNTargetRasPi -Wno-psabi
endif
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra
LDLIBS := -ljson-c -lpthread

GN_BASIC_SOURCES := \
  $(GnBasicLibDir)/GnBasics.cpp \
  $(GnBasicLibDir)/GnTime.cpp \
  $(GnBasicLibDir)/GnCoopTimer.cpp \
  $(GnBasicLibDir)/GnUdpStatus.cpp \
  $(GnBasicLibDir)/GnHeaterControl.cpp \
  $(GnBasicLibDir)/GnDeviceIdentity.cpp

COMMON_SOURCES := \
  src/Main.cpp \
  src/TimeSync.cpp \
  src/TimeConfigSync.cpp \
  src/Logger.cpp \
  src/ConfigStore.cpp \
  src/Scheduler.cpp \
  src/ButtonController.cpp \
  src/WebApplication.cpp \
  src/StatusApi.cpp \
  src/WebServerNative.cpp \
  src/WebSocketServerNative.cpp

ifeq ($(TARGET_SYSTEM),dummy)
  CPPFLAGS += -DSYSTEM_DUMMY
  PLATFORM_SOURCE := src/HardwareDummy.cpp
else ifeq ($(TARGET_SYSTEM),raspi)
  CPPFLAGS += -DSYSTEM_RASPI
  PLATFORM_SOURCE := src/HardwareRasPi.cpp
  LDLIBS += -lgpiod
else
  $(error Unsupported TARGET_SYSTEM '$(TARGET_SYSTEM)'; use dummy or raspi)
endif

SOURCES := $(COMMON_SOURCES) $(PLATFORM_SOURCE)
GN_BASIC_OBJECTS := $(patsubst $(GnBasicLibDir)/%.cpp,$(BUILD_DIR)/GnBasicLib/%.o,$(GN_BASIC_SOURCES))
OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES)) $(GN_BASIC_OBJECTS)
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all dummy raspi build clean full-clean

all: $(PROGRAM)

dummy:
	$(MAKE) TARGET_SYSTEM=dummy all

raspi:
	$(MAKE) TARGET_SYSTEM=raspi all

$(PROGRAM): $(OBJECTS) $(VERSION_HEADER)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

$(VERSION_HEADER): Versions.txt ../tools/UpdateVersionFiles.py
	python3 ../tools/UpdateVersionFiles.py .

$(OBJECTS): $(VERSION_HEADER)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/GnBasicLib/%.o: $(GnBasicLibDir)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	$(MAKE) clean
	$(MAKE) all

clean:
	rm -rf -- generated/build

full-clean: clean
	rm -f -- "$(PROGRAM)" 

-include $(DEPENDS)
