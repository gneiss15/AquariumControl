# Makefile, Version: 1.13

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Werror -Wno-psabi
CPPFLAGS ?= -I../Libs/GnBasicLib
TARGET   := DashboardServer
SOURCES  := FeedbackMonitor.cpp DashboardServer.cpp TimeStore.cpp HeatProfileStore.cpp DeviceRegistry.cpp DeviceEditor.cpp AssignmentControl.cpp EndpointAssignments.cpp
LDLIBS   := -ljson-c

.PHONY: all clean run versions ForceVersion

all: Version.inc.h $(TARGET)

ForceVersion:

Version.inc.h: Versions.txt ForceVersion
	python3 ../tools/UpdateVersionFiles.py .

versions: Version.inc.h

$(TARGET): Makefile $(SOURCES) FeedbackMonitor.h DeviceRegistry.h AssignmentControl.h TimeStore.h HeatProfileStore.h ../Libs/GnBasicLib/GnFunctionConfig.inc.h Version.inc.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) 8090 .

clean:
	rm -f $(TARGET)
