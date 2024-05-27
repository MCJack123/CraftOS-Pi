CIRCLESTDLIBHOME = /home/jack/Downloads/circle-stdlib
include $(CIRCLESTDLIBHOME)/Config.mk

CIRCLEHOME = $(CIRCLESTDLIBHOME)/libs/circle
NEWLIBDIR = $(CIRCLESTDLIBHOME)/install/$(NEWLIB_ARCH)

OBJS    = main/craftos2-lua/src/lapi.o main/craftos2-lua/src/lauxlib.o main/craftos2-lua/src/lbaselib.o main/craftos2-lua/src/lbitlib.o main/craftos2-lua/src/lcode.o main/craftos2-lua/src/lcorolib.o \
	main/craftos2-lua/src/lctype.o main/craftos2-lua/src/ldblib.o main/craftos2-lua/src/ldebug.o main/craftos2-lua/src/ldo.o main/craftos2-lua/src/ldump.o \
	main/craftos2-lua/src/lfunc.o main/craftos2-lua/src/lgc.o main/craftos2-lua/src/linit.o main/craftos2-lua/src/liolib.o main/craftos2-lua/src/llex.o main/craftos2-lua/src/lmathlib.o \
	main/craftos2-lua/src/lmem.o main/craftos2-lua/src/loadlib.o main/craftos2-lua/src/lobject.o main/craftos2-lua/src/lopcodes.o main/craftos2-lua/src/loslib.o main/craftos2-lua/src/lparser.o \
	main/craftos2-lua/src/lstate.o main/craftos2-lua/src/lstring.o main/craftos2-lua/src/lstrlib.o main/craftos2-lua/src/ltable.o main/craftos2-lua/src/ltablib.o main/craftos2-lua/src/ltm.o \
	main/craftos2-lua/src/lundump.o main/craftos2-lua/src/lutf8lib.o main/craftos2-lua/src/lvm.o main/craftos2-lua/src/lzio.o main/llock.o \
	main/api/fs_handle.o main/api/fs.o main/api/http.o main/api/os.o main/api/peripheral.o main/api/redstone.o main/api/term.o main/peripheral/speaker.o \
	main/main.o main/machine.o main/module/terminal.o main/module/redstone.o main/kernel.o main/event.o main/driver/hid.o main/module/httpclient.o main/module/yuarel.o

LIBS   := $(NEWLIBDIR)/lib/libm.a $(NEWLIBDIR)/lib/libc.a $(NEWLIBDIR)/lib/libcirclenewlib.a \
 	$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
 	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
 	$(CIRCLEHOME)/addon/wlan/libwlan.a \
 	$(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
  	$(CIRCLEHOME)/lib/usb/libusb.a \
 	$(CIRCLEHOME)/lib/input/libinput.a \
 	$(CIRCLEHOME)/lib/fs/libfs.a \
  	$(CIRCLEHOME)/lib/net/libnet.a \
  	$(CIRCLEHOME)/lib/sched/libsched.a \
  	$(CIRCLEHOME)/lib/sound/libsound.a \
  	$(CIRCLEHOME)/lib/libcircle.a \
	$(CIRCLESTDLIBHOME)/libs/mbedtls/library/libmbedcrypto.a \
	$(CIRCLESTDLIBHOME)/libs/mbedtls/library/libmbedtls.a \
	$(CIRCLESTDLIBHOME)/libs/mbedtls/library/libmbedx509.a \
	$(CIRCLESTDLIBHOME)/src/circle-mbedtls/libcircle-mbedtls.a

CFLAGS  = -DDEPTH=8 -Wno-error=incompatible-pointer-types
INCLUDE = -Wno-error=incompatible-pointer-types \
	-I$(CIRCLEHOME)/addon/fatfs \
	-Imain/craftos2-lua/include \
	-I "$(NEWLIBDIR)/include" \
	-I $(STDDEF_INCPATH) \
	-I $(CIRCLESTDLIBHOME)/include \
	-I $(CIRCLESTDLIBHOME)/libs/mbedtls/include

#OPTIMIZE = -O0

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
