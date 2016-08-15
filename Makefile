ROOT := $(dir $(lastword $(MAKEFILE_LIST)))
include ./Kite/Makefile.in

SOURCES += \
		   LazerSharks.cpp \

OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))
OBJECTS := $(patsubst %.c,%.o,$(OBJECTS))

CXXFLAGS += -I${ROOT} -I${ROOT}/inih/ -I${ROOT}/inih/cpp/

CXXFLAGS += -g
LDFLAGS  += -g

ALL: $(OBJECTS) example.exe

%.exe: $(OBJECTS) %.o
	$(CXX) $^ -o $@ $(LDFLAGS)

clean:
	rm -f  *.o *.exe $(OBJECTS)
