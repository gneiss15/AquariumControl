# Makefile, Version: 1.21

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Werror -Wno-psabi
CPPFLAGS ?= -I../Libs/GnBasicLib -I../Libs/GnAqPrjsLib
TARGET   := DashboardServer
SOURCES  := FeedbackMonitor.cpp DashboardServer.cpp TimeStore.cpp HeatProfileStore.cpp DeviceRegistry.cpp DeviceEditor.cpp AssignmentControl.cpp EndpointAssignments.cpp
LDLIBS   := -ljson-c

.PHONY: all clean run versions ForceVersion

all: Version.inc.h $(TARGET)

ForceVersion:

Version.inc.h: Versions.txt ForceVersion
	python3 ../tools/UpdateVersionFiles.py .

versions: Version.inc.h

$(TARGET): ../Libs/GnAqPrjsLib/GnUdpSchema.h $(wildcard ../Libs/GnAqPrjsLib/Definitions/Contract/*.inc.h) Makefile $(SOURCES) FeedbackMonitor.h ChargeLearningDebug.h ../Libs/GnAqPrjsLib/GnTemporaryDiagnostics.h DeviceRegistry.h AssignmentControl.h TimeStore.h HeatProfileStore.h $(wildcard ../Libs/GnAqPrjsLib/GnUdp*.h) $(wildcard ../Libs/GnAqPrjsLib/GnJson*.h) ../Libs/GnBasicLib/GnWireJson.h ../Libs/GnAqPrjsLib/GnFunctionConfig.inc.h Version.inc.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) 8090 .

clean:
	rm -f $(TARGET)
