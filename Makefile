#
# cross-tools definitions
#
ifeq '$(findstring ;,$(PATH))' ';'
    detected_OS := Windows
else
    detected_OS := $(shell uname 2>/dev/null || echo Unknown)
    detected_OS := $(patsubst CYGWIN%,Cygwin,$(detected_OS))
    detected_OS := $(patsubst MSYS%,MSYS,$(detected_OS))
    detected_OS := $(patsubst MINGW%,MSYS,$(detected_OS))
endif

ifeq ($(DEB_TARGET_ARCH),armel)
CROSS_COMPILE=arm-linux-gnueabi-
endif

CXX=$(CROSS_COMPILE)g++
CXX_PATH := $(shell which $(CROSS_COMPILE)g++-4.7)

CC=$(CROSS_COMPILE)gcc
CC_PATH := $(shell which $(CROSS_COMPILE)gcc-4.7)

ifneq ($(CXX_PATH),)
	CXX=$(CROSS_COMPILE)g++-4.7
endif

ifneq ($(CC_PATH),)
	CC=$(CROSS_COMPILE)gcc-4.7
endif

ifeq ($(detected_OS),Cygwin)
    NAME_SUFFIX=.exe
    TEST_CFLAGS=-DWIN32
else
	NAME_SUFFIX=
endif

#
# custom definitions
#
#INCLUDES=-I../../ext_cygwin/include
#LIBS=-L../../ext_cygwin/lib -lmosquittopp -lmosquitto -ljsoncpp

LIBS=-lpthread



ifneq ($(BUILD_DEBUG),)
	DEBUG_CFLAGS=-DDEBUG_BUILD -g -O0 -fno-omit-frame-pointer
	DEBUG_LDFLAGS=-g -O0
else
	DEBUG_CFLAGS=-O3 -Os
	DEBUG_LDFLAGS=-O3 -Os
endif


MAIN_OBJ=pwm-player.o


#CFLAGS=-Wall -std=c++0x -Os -I.
CFLAGS=-Wall -std=c++0x -pthread -I. -D_GNU_SOURCE $(INCLUDES) $(DEBUG_CFLAGS) $(TEST_CFLAGS)
LDFLAGS= $(DEBUG_LDFLAGS) $(LIBS)

MP_BIN=$(NAME_PREF)pwm-player$(NAME_SUFFIX)

.PHONY: all clean

HDRS=\
MIDIEvent.h \
MIDIFileReader.h


OBJS=\
$(MAIN_OBJ) \
pwm-player-midi.o \
pwm-player-melody.o \
MIDIFileReader.o

all : $(MP_BIN)

$(OBJS): %.o: %.cpp $(HDRS)
	@echo Compiling $<
	${CXX} -c $< -o $@ ${CFLAGS}

$(MP_BIN) : $(OBJS)
	${CXX} $^ ${LDFLAGS} -o $@

.PHONY: all clean

clean :
	-rm -f $(OBJS) $(MP_BIN) *.log

install: all
ifeq ($(BUILD_TEST),)
	install -d $(DESTDIR)
	install -d $(DESTDIR)/usr/bin

	install -m 0755  $(MP_BIN) $(DESTDIR)/usr/bin/$(MP_BIN)
endif
