CIRCLESTDLIBHOME ?= /home/jack/Downloads/circle-arch/raspi1
include $(CIRCLESTDLIBHOME)/Config.mk

CIRCLEHOME = $(CIRCLESTDLIBHOME)/libs/circle
NEWLIBDIR = $(CIRCLESTDLIBHOME)/install/$(NEWLIB_ARCH)

OBJS    = \
	main/craftos-base/craftos2-lua/src/error_info.o \
    main/craftos-base/craftos2-lua/src/lapi.o \
    main/craftos-base/craftos2-lua/src/lcode.o \
    main/craftos-base/craftos2-lua/src/lctype.o \
    main/craftos-base/craftos2-lua/src/ldebug.o \
    main/craftos-base/craftos2-lua/src/ldo.o \
    main/craftos-base/craftos2-lua/src/ldump.o \
    main/craftos-base/craftos2-lua/src/lfunc.o \
    main/craftos-base/craftos2-lua/src/lgc.o \
    main/craftos-base/craftos2-lua/src/linit.o \
    main/craftos-base/craftos2-lua/src/llex.o \
    main/craftos-base/craftos2-lua/src/lmem.o \
    main/craftos-base/craftos2-lua/src/lobject.o \
    main/craftos-base/craftos2-lua/src/lopcodes.o \
    main/craftos-base/craftos2-lua/src/lparser.o \
    main/craftos-base/craftos2-lua/src/lstate.o \
    main/craftos-base/craftos2-lua/src/lstring.o \
    main/craftos-base/craftos2-lua/src/ltable.o \
    main/craftos-base/craftos2-lua/src/ltm.o \
    main/craftos-base/craftos2-lua/src/lundump.o \
    main/craftos-base/craftos2-lua/src/lvm.o \
    main/craftos-base/craftos2-lua/src/lzio.o \
    main/craftos-base/craftos2-lua/src/lauxlib.o \
    main/craftos-base/craftos2-lua/src/lbaselib.o \
    main/craftos-base/craftos2-lua/src/lbitlib.o \
    main/craftos-base/craftos2-lua/src/lcorolib.o \
    main/craftos-base/craftos2-lua/src/ldblib.o \
    main/craftos-base/craftos2-lua/src/liolib.o \
    main/craftos-base/craftos2-lua/src/lmathlib.o \
    main/craftos-base/craftos2-lua/src/loslib.o \
    main/craftos-base/craftos2-lua/src/lstrlib.o \
    main/craftos-base/craftos2-lua/src/ltablib.o \
    main/craftos-base/craftos2-lua/src/lutf8lib.o \
    main/craftos-base/craftos2-lua/src/loadlib.o \
	main/craftos-base/src/event.o \
	main/craftos-base/src/funcs.o \
	main/craftos-base/src/machine.o \
	main/craftos-base/src/mmfs.o \
	main/craftos-base/src/string_list.o \
	main/craftos-base/src/terminal.o \
	main/craftos-base/src/apis/fs_handle.o \
	main/craftos-base/src/apis/fs.o \
	main/craftos-base/src/apis/http_handle.o \
	main/craftos-base/src/apis/http.o \
	main/craftos-base/src/apis/os.o \
	main/craftos-base/src/apis/peripheral.o \
	main/craftos-base/src/apis/redstone.o \
	main/craftos-base/src/apis/term.o \
	main/speaker.o \
	main/main.o \
	main/machine.o \
	main/kernel.o \
	main/event.o \
	main/hid.o \
	main/httpclient.o \
	main/yuarel.o

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
	-Imain \
	-Imain/craftos-base/include \
	-Imain/craftos-base/craftos2-lua/include \
	-Imain/craftos-base/craftos2-lua/src \
	-I "$(NEWLIBDIR)/include" \
	-I $(STDDEF_INCPATH) \
	-I $(CIRCLESTDLIBHOME)/include \
	-I $(CIRCLESTDLIBHOME)/libs/mbedtls/include

#OPTIMIZE = -O0

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
