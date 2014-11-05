# Makefile for "rfidscan-lib" and "rfidscan-tool"
#
# Works on Mac OS X, Windows, Linux, and other Linux-like systems.
# Type "make help" to see supported platforms.
#  
# Build arguments:
# - "OS=macosx"  -- build Mac version on Mac OS X
# - "OS=windows" -- build Windows version on Windows
# - "OS=linux"   -- build Linux version on Linux
# 
# Architecture is usually detected automatically, so normally just type "make".
#
# Dependencies: 
# - hidapi (included), which uses libusb on Linux-like OSes
#
# Platform-specific notes:
#
# Mac OS X 
#   - Install Xcode with "Unix Dev Support" and "Command-line tools" (in Preferences)
#   - make
#
# Windows XP/7  
#   - Install MinGW and MSYS (http://www.tdragon.net/recentgcc/ )
#   - make
#
# Linux (Ubuntu) 
#   - apt-get install build-essential pkg-config libusb-1.0-0-dev
#   - make
#
# Linux (Fedora 18+)
#   - yum install make gcc 
#   - make 
#
# Linux (Fedora 17)
#   - yum install make gcc libusb1-static glibc-static
#   - make
#
# FreeBSD
#   - libusb is part of the OS so no pkg-config needed.
#   - No -ldl on FreeBSD necessary.
#   - For FreeBSD versions < 10, iconv is a package that needs to be installed;
#     in this case it lives in /usr/local/lib/
#
# Linux Ubuntu 32-bit cross-compile on 64-bit
#   To build 32-bit on 64-bit Ubuntu, try a chrooted build:
#   (warning this will use up a lot of disk space)
#   - sudo apt-get install ubuntu-dev-tools
#   - pbuilder-dist oneiric i386 create
#   - mkdir $HOME/i386
#   - cp -r rfidscan $HOME/i386
#   - pbuilder-dist oneiric i386 login --bindmounts $HOME/i386
#     (now in the chrooted area)
#   - apt-get install libusb-1.0-0 libusb-1.0-0-dev
#   - cd $HOME/i386/rfidscan
#   - CFLAGS='-I/usr/include/libusb-1.0' LIBS='-lusb-1.0' make
#   - exit
#   
# Raspberry Pi
#   - apt-get install libusb-1.0.0-dev
#   - make
#
# BeagleBone / BeagleBoard (on Angstrom Linux)
#   - opkg install libusb-0.1-4-dev  (FIXME: uses HIDAPI & libusb-1.0 now)	
#   - May need to symlink libusb 
#      cd /lib; ln -s libusb-0.1.so.4 libusb.so
#   - make
#
#

#
# rfidscan-tool uses the HIDAPI to communicate with the RFID Scanner Device
# HIDAPI is available on Mac, Windows, Linux... 
# The dependencies are iconv, libusb-1.0, pthread, dl
#

USBLIB_TYPE ?= HIDAPI

# uncomment for debugging HID stuff
# or do "CFLAGS=-DDEBUG_PRINTF make"
CFLAGS += -DDEBUG_PRINTF


# try to do some autodetecting
UNAME := $(shell uname -s)

ifeq "$(UNAME)" "Darwin"
	OS=macosx
endif

ifeq "$(OS)" "Windows_NT"
	OS=windows
endif

ifeq "$(UNAME)" "Linux"
	OS=linux
endif

ifeq "$(UNAME)" "FreeBSD"
	OS=freebsd
endif


GIT_TAG="$(strip $(shell git tag | tail -1))"
MACH_TYPE="$(strip $(shell uname -m))"

RFIDSCAN_VERSION="$(GIT_TAG)-$(OS)-$(MACH_TYPE)"




#################  Mac OS X  ##################################################
ifeq "$(OS)" "macosx"
LIBTARGET = libRfidScan.dylib
CFLAGS += -mmacosx-version-min=10.6 
#CFLAGS += -fsanitize=address

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -arch i386 -arch x86_64
# don't need pthread with clang
#CFLAGS += -pthread
CFLAGS += -O2 -D_THREAD_SAFE -MT MD -MP 
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/mac/hid.o
endif

LIBS += -framework IOKit -framework CoreFoundation

EXEFLAGS = 
#LIBFLAGS = -bundle -o $(LIBTARGET) -Wl,-search_paths_first $(LIBS)
LIBFLAGS = -dynamiclib -o $(LIBTARGET) -Wl,-search_paths_first $(LIBS)
EXE=

INSTALL = install -D
EXELOCATION ?= /usr/local/bin
LIBLOCATION ?= /usr/local/lib
INCLOCATION ?= /usr/local/include

endif

#################  Windows  ##################################################
ifeq "$(OS)" "windows"
LIBTARGET = rfidscan-lib.dll
#LIBS +=  -mwindows -lsetupapi -Wl,--enable-auto-import -static-libgcc -static-libstdc++ -lkernel32 
#LIBS +=  -mwindows -lsetupapi -Wl,-Bdynamic -lgdi32 -Wl,--enable-auto-import -static-libgcc -static-libstdc++ -lkernel32
LIBS +=             -lsetupapi -Wl,--enable-auto-import -static-libgcc -static-libstdc++ 

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/windows/hid.o
endif

EXEFLAGS =
#LIBFLAGS = -shared -o $(LIBTARGET) -Wl,--add-stdcall-alias -Wl,--export-all-symbols -Wl,--out-implib,$(LIBTARGET).a $(LIBS)
LIBFLAGS = -shared -o $(LIBTARGET) -Wl,--add-stdcall-alias -Wl,--export-all-symbols,--output-def,rfidscan-lib.def,--out-implib,rfidscan-lib.a
EXE= .exe

# this generates a rfidscan-lib.lib for use with MSVC
LIB_EXTRA = lib /machine:i386 /def:rfidscan-lib.def

INSTALL = cp
EXELOCATION ?= $(SystemRoot)/system32
LIBLOCATION ?= $(SystemRoot)/system32
# not sure where this really should point
INCLOCATION ?= $(SystemRoot)/system32

endif

#################  Linux  ####################################################
ifeq "$(OS)" "linux"
LIBTARGET = librfidscan.so
# was rfidscan-lib.so

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/libusb/hid.o
CFLAGS += `pkg-config libusb-1.0 --cflags` -fPIC
LIBS   += `pkg-config libusb-1.0 --libs` -lrt -lpthread -ldl
endif

# static doesn't work on Ubuntu 13+
#EXEFLAGS = -static
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

INSTALL = install -D
EXELOCATION ?= /usr/local/bin
LIBLOCATION ?= /usr/local/lib
INCLOCATION ?= /usr/local/include

endif

################  FreeBSD  ###################################################
ifeq "$(OS)" "freebsd"
LIBTARGET = librfidscan.so
# was rfidscan-lib.so

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/libusb/hid.o
CFLAGS += -I/usr/local/include -fPIC
LIBS   += -lusb -lrt -lpthread
ifndef FBSD10
LIBS   += -L/usr/local/lib -liconv
endif
endif

# Static binaries don't play well with the iconv implementation of FreeBSD 10
ifndef FBSD10
EXEFLAGS = -static
endif
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

INSTALL = install -D
EXELOCATION ?= /usr/local/bin
LIBLOCATION ?= /usr/local/lib
INCLOCATION ?= /usr/local/include

endif

#################  WRT Linux  ################################################
ifeq "$(OS)" "wrt"
LIBTARGET = rfidscan-lib.so

ifeq "$(USBLIB_TYPE)" "HIDAPI"
CFLAGS += -DUSE_HIDAPI
CFLAGS += -I./hidapi/hidapi 
OBJS = ./hidapi/libusb/hid.o
CFLAGS += `pkg-config libusb-1.0 --cflags` -fPIC 
LIBS   += `pkg-config libusb-1.0 --libs` -lrt -lpthread -ldl 
endif

#EXEFLAGS = -static
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

endif

##############  Cross-compile WRT Linux  #####################################
ifeq "$(OS)" "wrtcross"

EXEFLAGS = -static
LIBFLAGS = -shared -o $(LIBTARGET) $(LIBS)
EXE=

endif



#####################  Common  ###############################################


#CFLAGS += -O -Wall -std=gnu99 -I ../hardware/firmware 
CFLAGS += -std=gnu99 
CFLAGS += -g
CFLAGS += -DRFIDSCAN_VERSION=\"$(RFIDSCAN_VERSION)\"

OBJS +=  rfidscan-lib.o 


PKGOS = $(RFIDSCAN_VERSION)

all: msg rfidscan-tool lib 

# symbolic targets:
help:
	@echo "This Makefile works on multiple archs. Use one of the following:"
	@echo "make            ... autodetect platform and build appropriately"
	@echo "make OS=windows ... build Windows  rfidscan-lib and rfidscan-tool" 
	@echo "make OS=linux   ... build Linux    rfidscan-lib and rfidscan-tool" 
	@echo "make OS=freebsd ... build FreeBSD    rfidscan-lib and rfidscan-tool" 
	@echo "make OS=macosx  ... build Mac OS X rfidscan-lib and rfidscan-tool" 
	@echo "make OS=wrt     ... build OpenWrt rfidscan-lib and rfidscan-tool"
	@echo "make OS=wrtcross... build for OpenWrt using cross-compiler"
	@echo "make lib        ... build rfidscan-lib shared library"
	@echo "make package    ... zip up rfidscan-tool and rfidscan-lib "
	@echo "make clean      ... delete build products, leave binaries & libs"
	@echo "make distclean  ... delele binaries and libs too"
	@echo

msg: 
	@echo "Building for OS=$(OS) RFIDSCAN_VERSION=$(RFIDSCAN_VERSION)"


$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

rfidscan-tool: $(OBJS) rfidscan-tool.o
	$(CC) $(CFLAGS) -c rfidscan-tool.c -o rfidscan-tool.o
	$(CC) $(CFLAGS) $(EXEFLAGS) -g $(OBJS) $(LIBS) rfidscan-tool.o -o rfidscan-tool$(EXE) 

lib: $(OBJS)
	$(CC) $(LIBFLAGS) $(CFLAGS) $(OBJS) $(LIBS)
	$(LIB_EXTRA)

package: lib rfidscan-tool
	@echo "Packaging up rfidscan-tool and rfidscan-lib for '$(PKGOS)'"
	zip rfidscan-tool-$(PKGOS).zip rfidscan-tool$(EXE)
	zip rfidscan-lib-$(PKGOS).zip $(LIBTARGET) rfidscan-lib.h
	@#mkdir -f builds && cp rfidscan-tool-$(PKGOKS).zip builds

install: all
	$(INSTALL) rfidscan-tool$(EXE) $(DESTDIR)$(EXELOCATION)/rfidscan-tool$(EXE)
	$(INSTALL) $(LIBTARGET) $(DESTDIR)$(LIBLOCATION)/$(LIBTARGET)
	$(INSTALL) rfidscan-lib.h $(DESTDIR)$(INCLOCATION)/rfidscan-lib.h

.PHONY: install

clean: 
	rm -f $(OBJS)
	rm -f $(LIBTARGET)
	rm -f rfidscan-tool.o

distclean: clean
	rm -f rfidscan-tool$(EXE)
	rm -f $(LIBTARGET) $(LIBTARGET).a

# show shared library use
# in general we want minimal to no dependecies for rfidscan-tool

# shows shared lib usage on Mac OS X
otool:
	otool -L rfidscan-tool
# show shared lib usage on Linux
ldd:
	ldd rfidscan-tool
# show shared lib usage on Windows
# FIXME: only works inside command prompt from
# Start->All Programs-> MS Visual Studio 2012 -> VS Tools -> Devel. Cmd Prompt
dumpbin: 
	dumpbin.exe /exports $(LIBTARGET)
	dumpbin.exe /exports rfidscan-tool.exe


printvars:
	@echo "OS=$(OS), CFLAGS=$(CFLAGS), LDFLAGS=$(LDFLAGS), LIBS=$(LIBS), LIBFLAGS=$(LIBFLAGS)"
