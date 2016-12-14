#
# Makefile for ZeroTier SDK on Linux
#
# Targets
#   all: build every target possible on host system, plus tests
#   check: reports OK/FAIL of built targets
#   tests: build only test applications for host system
#   clean: removes all built files, objects, other trash

# Target output filenames
SHARED_LIB_NAME    = libztlinux.so
SDK_INTERCEPT_NAME = libztintercept.so
SDK_SERVICE_NAME   = zerotier-sdk-service
ONE_SERVICE_NAME   = zerotier-one
ONE_CLI_NAME       = zerotier-cli
ONE_ID_TOOL_NAME   = zerotier-idtool
LWIP_LIB_NAME      = liblwip.so
PICO_LIB_NAME      = libpicotcp.so
#
SHARED_LIB         = $(BUILD)/$(SHARED_LIB_NAME)
SDK_INTERCEPT      = $(BUILD)/$(SDK_INTERCEPT_NAME)
SDK_SERVICE        = $(BUILD)/$(SDK_SERVICE_NAME)
ONE_SERVICE        = $(BUILD)/$(ONE_SERVICE_NAME)
ONE_CLI            = $(BUILD)/$(ONE_CLI_NAME)
ONE_IDTOOL         = $(BUILD)/$(ONE_IDTOOL_NAME)
LWIP_LIB           = $(BUILD)/$(LWIP_LIB_NAME)
PICO_LIB           = $(BUILD)/$(PICO_LIB_NAME)
#
LWIP_DIR      = ext/lwip
PICOTCP_DIR   = ext/picotcp
# 
LWIP_DRIVER_FILES  = src/stack_drivers/lwip/lwip.cpp 
PICO_DRIVER_FILES  = src/stack_drivers/picotcp/picotcp.cpp 
SDK_SERVICE_CPP_FILES:=src/tap.cpp \
					    src/proxy.cpp \
					    $(ZT1)/service/OneService.cpp \
					    $(ZT1)/one.cpp 

SDK_SERVICE_C_FILES = src/rpc.c
SDK_INTERCEPT_C_FILES:=src/sockets.c \
						src/intercept.c \
						src/rpc.c

ZTFLAGS:=-DZT_ONE_NO_ROOT_CHECK -DSDK


# Automagically pick clang or gcc, with preference for clang
# This is only done if we have not overridden these with an environment or CLI variable
ifeq ($(origin CC),default)
	CC=$(shell if [ -e /usr/bin/clang ]; then echo clang; else echo gcc; fi)
endif
ifeq ($(origin CXX),default)
	CXX=$(shell if [ -e /usr/bin/clang++ ]; then echo clang++; else echo g++; fi)
endif

#UNAME_M=$(shell $(CC) -dumpmachine | cut -d '-' -f 1)

INCLUDES?=
DEFS?=
LDLIBS?=
CFLAGS=

include objects.mk

ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	CXXFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	LDFLAGS=-ldl
	STRIP?=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in -O0 even on a 3ghz box!
ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-O3 -fstack-protector
	CFLAGS+=-Wall -fPIE -fvisibility=hidden -pthread $(INCLUDES) -DNDEBUG $(DEFS)
	CXXFLAGS?= -fstack-protector
	CXXFLAGS+=-Wall -Wreorder -fPIE -fvisibility=hidden -fno-rtti -pthread $(INCLUDES) -DNDEBUG $(DEFS) -std=c++11
	LDFLAGS=-ldl -pie -Wl,-z,relro,-z,now
	STRIP?=strip
	STRIP+=--strip-all
endif

# Debug output for ZeroTier service
ifeq ($(ZT_TRACE),1)
	DEFS+=-DZT_TRACE
endif

CXXFLAGS+=-std=c++11

INCLUDES+= -Iext \
	-I$(ZT1)/osdep \
	-I$(ZT1)/node \
	-I$(ZT1)/service \
	-I../$(ZT1)/osdep \
	-I../$(ZT1)/node \
	-I../$(ZT1)/service \
	-I. \
	-Isrc \
	-Isrc/stack_drivers \
	-I$(LWIP_DIR)/src/include \
	-I$(LWIP_DIR)/src/include/ipv4 \
	-I$(LWIP_DIR)/src/include/ipv6 \
	-I$(PICOTCP_DIR)/include \
	-I$(PICOTCP_DIR)/build/include \
	-Isrc/stack_drivers/lwip \
	-Isrc/stack_drivers/picotcp \
	-Isrc/stack_drivers/jip



# Stack selection / parameters

# lwIP debug
ifeq ($(SDK_LWIP_DEBUG),1)
	LWIP_FLAGS+=SDK_LWIP_DEBUG=1
endif

# picoTCP debug
ifeq ($(SDK_PICOTCP_DEBUG),1)
	PICOTCP_FLAGS+=DEBUG=1
else
	PICOTCP_FLAGS+=DEBUG=0
endif

# lwIP
ifeq ($(SDK_LWIP),1)
	STACK_FLAGS+=-DSDK_LWIP
endif

# picoTCP
ifeq ($(SDK_PICOTCP),1)
	STACK_FLAGS+=-DSDK_PICOTCP
endif

# jip
ifeq ($(SDK_JIP),1)
	STACK_FLAGS+=-DSDK_JIP
endif



# TCP protocol version
ifeq ($(SDK_IPV4),1)
	LWIP_FLAGS+=SDK_IPV4=1
	STACK_FLAGS+=-DSDK_IPV4
endif

ifeq ($(SDK_IPV6),1)
	LWIP_FLAGS+=SDK_IPV6=1
	STACK_FLAGS+=-DSDK_IPV6 
endif



# Debug output for the SDK
# Specific levels can be controlled in src/debug.h
ifeq ($(SDK_DEBUG),1)
	DEFS+=-DSDK_DEBUG -g
endif
# Log debug chatter to file, path is determined by environment variable ZT_SDK_LOGFILE
ifeq ($(SDK_DEBUG_LOG_TO_FILE),1)
	DEFS+=-DSDK_DEBUG_LOG_TO_FILE
endif


# ------ MISC TARGETS ------

all: remove_only_intermediates linux_shared_lib check

remove_only_intermediates:
	-find . -type f \( -name '*.o' -o -name '*.so' \) -delete


# --- EXTERNAL LIBRARIES ---
lwip:
	mkdir -p build
	-make -f make-liblwip.mk $(LWIP_FLAGS)

pico:
	mkdir -p build
	cd ext/picotcp; make lib $(SDK_PICOTCP_FLAGS) PICO_SUPPORT_UDP=1 ARCH=shared IPV4=1 IPV6=1
	$(CC) -g -nostartfiles -shared -o ext/picotcp/build/lib/libpicotcp.so ext/picotcp/build/lib/*.o ext/picotcp/build/modules/*.o
	cp ext/picotcp/build/lib/libpicotcp.so build/libpicotcp.so

jip:
	-make -f make-jip.mk $(JIP_FLAGS)


# --------- LINUX ----------
# Build everything
linux: one linux_service_and_intercept linux_shared_lib

# Build vanilla ZeroTier One binary
one: $(OBJS) $(ZT1)/service/OneService.o $(ZT1)/one.o $(ZT1)/osdep/LinuxEthernetTap.o
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BUILD)/zerotier-one $(OBJS) $(ZT1)/service/OneService.o $(ZT1)/one.o $(ZT1)/osdep/LinuxEthernetTap.o $(LDLIBS)
	$(STRIP) $(ONE_SERVICE)
	cp $(ONE_SERVICE) $(INT)/docker/docker_demo/$(ONE_SERVICE_NAME)

# Build only the intercept library
linux_intercept:
	# Use gcc not clang to build standalone intercept library since gcc is typically used for libc and we want to ensure maximal ABI compatibility
	gcc $(DEFS) $(INCLUDES) -g -O2 -Wall -std=c99 -fPIC -DVERBOSE -D_GNU_SOURCE -DSDK_INTERCEPT -nostdlib -nostdlib -shared -o $(SDK_INTERCEPT) $(SDK_INTERCEPT_C_FILES) -ldl

# Build only the SDK service
ifeq ($(SDK_LWIP),1)
linux_sdk_service: lwip $(OBJS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(STACK_FLAGS) $(DEFS) $(INCLUDES) $(ZTFLAGS) -DSDK_SERVICE -o $(SDK_SERVICE) $(OBJS) $(LWIP_DRIVER_FILES) $(SDK_SERVICE_CPP_FILES) $(SDK_SERVICE_C_FILES) $(LDLIBS) -ldl
else
linux_sdk_service: pico $(OBJS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(STACK_FLAGS) $(DEFS) $(INCLUDES) $(ZTFLAGS) -DSDK_SERVICE -o $(SDK_SERVICE) $(OBJS) $(PICO_DRIVER_FILES) $(SDK_SERVICE_CPP_FILES) $(SDK_SERVICE_C_FILES) $(LDLIBS) -ldl
endif
	ln -sf $(SDK_SERVICE_NAME) $(BUILD)/zerotier-cli
	ln -sf $(SDK_SERVICE_NAME) $(BUILD)/zerotier-idtool


# Build both intercept library and SDK service (separate)
linux_service_and_intercept: linux_intercept linux_sdk_service

# Builds a single shared library which contains everything
linux_shared_lib: pico $(OBJS)
	$(CXX) $(CXXFLAGS) $(STACK_FLAGS) $(DEFS) $(INCLUDES) $(ZTFLAGS) -DSDK_SERVICE -DSDK -DSDK_BUNDLED -DSDK_DEBUG -DSDK_PICOTCP -DSDK_IPV4 $(PICO_DRIVER_FILES) $(SDK_INTERCEPT_C_FILES) $(SDK_SERVICE_CPP_FILES) src/service.cpp -c 
	ar -rcs libzt.a picotcp.o proxy.o tap.o one.o OneService.o service.o sockets.o rpc.o intercept.o OneService.o $(OBJS)

# Builds zt-embedded tests
linux_shared_lib_tests:
	$(CXX) -DSDK_SERVICE -DSDK -DSDK_BUNDLED -DSDK_DEBUG -DSDK_PICOTCP -DSDK_IPV4 $(CXXFLAGS) $(LDFLAGS) $(INCLUDES) -Isrc tests/api_test/zt_tcpserver4.c -o zt_tcpserver4.out -L. -lzt -ldl
	$(CXX) -DSDK_SERVICE -DSDK -DSDK_BUNDLED -DSDK_DEBUG -DSDK_PICOTCP -DSDK_IPV4 -DSDK_DEBUG $(CXXFLAGS) $(LDFLAGS) $(INCLUDES) -Isrc tests/api_test/zt_tcpclient4.c -o zt_tcpclient4.out -L. -lzt -ldl

# -------- ANDROID ---------
# TODO: CHECK if ANDROID/GRADLE TOOLS are installed
# Build library for Android Unity integrations
# Build JNI library for Android app integration
android_jni_lib:
	cd $(INT)/android/android_jni_lib/proj; ./gradlew assembleDebug
	mkdir -p $(BUILD)/android_jni_lib
	cp docs/android_zt_sdk.md $(BUILD)/android_jni_lib/README.md
	mv -f $(INT)/android/android_jni_lib/java/libs/* $(BUILD)/android_jni_lib
	cp -R $(BUILD)/android_jni_lib/* $(INT)/android/example_app/app/src/main/jniLibs




# -------- TESTING ---------

unit_test: one linux_service_and_intercept
	mkdir -p $(BUILD)
	cp $(SDK_INTERCEPT) tests/unit/docker/$(SDK_INTERCEPT_NAME)
	cp $(SDK_SERVICE) tests/unit/docker/$(SDK_SERVICE_NAME)
	cp $(LWIP_LIB) tests/unit/docker/$(LWIP_LIB_NAME)
	cp $(ONE_CLI) tests/unit/docker/$(ONE_CLI_NAME)
	cp $(ONE_SERVICE) tests/unit/docker/$(ONE_SERVICE_NAME)
	touch tests/unit/docker/docker_demo.name
	# Server image
	# This image will contain the server application and everything required to 
	# run the ZeroTier SDK service
	cd tests/unit/docker; docker build --tag="docker_demo" -f sdk_dockerfile .
	# Client image
	# This image is merely a test image designed to interact with the server image
	# in order to verify it's working properly
	cd tests/unit/docker; docker build --tag="docker_demo_monitor" -f monitor_dockerfile .

# Build the docker demo images
docker_demo: one linux_service_and_intercept
	mkdir -p $(BUILD)
	cp $(SDK_INTERCEPT) $(INT)/docker/docker_demo/$(SDK_INTERCEPT_NAME)
	cp $(SDK_SERVICE) $(INT)/docker/docker_demo/$(SDK_SERVICE_NAME)
	cp $(LWIP_LIB) $(INT)/docker/docker_demo/$(LWIP_LIB_NAME)
	cp $(ONE_CLI) $(INT)/docker/docker_demo/$(ONE_CLI_NAME)
	touch $(INT)/docker/docker_demo/docker_demo.name
	# Server image
	# This image will contain the server application and everything required to 
	# run the ZeroTier SDK service
	cd $(INT)/docker/docker_demo; docker build --tag="docker_demo" -f sdk_dockerfile .
	# Client image
	# This image is merely a test image designed to interact with the server image
	# in order to verify it's working properly
	cd $(INT)/docker/docker_demo; docker build --tag="docker_demo_monitor" -f monitor_dockerfile .

# Builds all docker test images
docker_images: one linux_service_and_intercept
	./tests/docker/build_images.sh

# Runs docker container tests
docker_test:
	./tests/docker/test.sh

# Checks the results of the docker tests
docker_check_test:
	./tests/docker/check.sh

# Check for the presence of built frameworks/bundles/libaries
check:
	-./check.sh $(LWIP_LIB)
	-./check.sh $(PICO_LIB)
	-./check.sh $(SDK_INTERCEPT)
	-./check.sh $(ONE_SERVICE)
	-./check.sh $(SDK_SERVICE)
	-./check.sh $(SHARED_LIB)
	-./check.sh $(BUILD)/android_jni_lib/arm64-v8a/libZeroTierOneJNI.so
	-./check.sh $(BUILD)/android_jni_lib/armeabi/libZeroTierOneJNI.so
	-./check.sh $(BUILD)/android_jni_lib/armeabi-v7a/libZeroTierOneJNI.so
	-./check.sh $(BUILD)/android_jni_lib/mips/libZeroTierOneJNI.so
	-./check.sh $(BUILD)/android_jni_lib/mips64/libZeroTierOneJNI.so
	-./check.sh $(BUILD)/android_jni_lib/x86/libZeroTierOneJNI.so
	-./check.sh $(BUILD)/android_jni_lib/x86_64/libZeroTierOneJNI.so

# Tests
OSTYPE=$(shell uname -s | tr '[A-Z]' '[a-z]')
TEST_OBJDIR := $(BUILD)/tests
TEST_SOURCES := $(wildcard tests/api_test/*.c)
TEST_TARGETS := $(addprefix $(BUILD)/tests/$(OSTYPE).,$(notdir $(TEST_SOURCES:.c=.out)))

$(BUILD)/tests/$(OSTYPE).%.out: tests/api_test/%.c
	-$(CC) $(CC_FLAGS) -o $@ $<

$(TEST_OBJDIR):
	mkdir -p $(TEST_OBJDIR)

tests: $(TEST_OBJDIR) $(TEST_TARGETS)
	mkdir -p $(BUILD)/tests; 
	mkdir -p build/tests/zerotier

test_suite: tests lwip linux_service_and_intercept
	cp tests/api_test/test.sh $(BUILD)/tests/test.sh
	cp tests/api_test/servers.sh $(BUILD)/tests/servers.sh
	cp tests/api_test/clients.sh $(BUILD)/tests/clients.sh
	cp tests/cleanup.sh $(BUILD)/tests/cleanup.sh
	cp $(LWIP_LIB) $(BUILD)/tests/zerotier/$(LWIP_LIB_NAME)

shared_lib_tests:
	mkdir -p $(BUILD)/tests;
	-$(CC) $(CC_FLAGS) -Isrc tests/api_test/zt_tcpserver4.c -o $(BUILD)/tests/zt_tcpserver4.out $(BUILD)/libztlinux.so

# ----- ADMINISTRATIVE -----

clean_android:
	# android JNI lib project
	-test -s /usr/bin/javac || { echo "Javac not found"; exit 1; }
	-cd $(INT)/android/android_jni_lib/proj; ./gradlew clean
	-rm -rf $(INT)/android/android_jni_lib/proj/build
	# example android app project
	-cd $(INT)/android/example_app; ./gradlew clean

clean_basic:
	-rm -rf $(BUILD)/*
	-rm -rf $(INT)/Unity3D/Assets/Plugins/*
	-rm -rf zerotier-cli zerotier-idtool
	-find . -type f \( -name $(ONE_SERVICE_NAME) -o -name $(SDK_SERVICE_NAME) -o -name $(ONE_CLI_NAME) \) -delete
	-find . -type f \( -name '*.a' -o -name '*.o' -o -name '*.so' -o -name '*.o.d' -o -name '*.out' -o -name '*.log' -o -name '*.dSYM' \) -delete

clean_thorough: clean_basic
	-rm -rf .depend

clean: clean_basic

clean_for_production:
	-find . -type f \( -name '*.identity'\) -delete