#
# Makefile for ZeroTier SDK on OSX
#
# Targets
#   all: build every target possible on host system, plus tests
#   check: reports OK/FAIL of built targets
#   tests: build only test applications for host system
#   clean: removes all built files, objects, other trash

# Target output filenames
SHARED_LIB_NAME    = libztosx.so
INTERCEPT_NAME     = libztintercept.so
SDK_SERVICE_NAME   = zerotier-sdk-service
ONE_SERVICE_NAME   = zerotier-one
ONE_CLI_NAME       = zerotier-cli
ONE_ID_TOOL_NAME   = zerotier-idtool
LWIP_LIB_NAME      = liblwip.so
#
SHARED_LIB         = $(BUILD)/$(SHARED_LIB_NAME)
INTERCEPT          = $(BUILD)/$(INTERCEPT_NAME)
SDK_SERVICE        = $(BUILD)/$(SDK_SERVICE_NAME)
ONE_SERVICE        = $(BUILD)/$(ONE_SERVICE_NAME)
ONE_CLI            = $(BUILD)/$(ONE_CLI_NAME)
ONE_IDTOOL         = $(BUILD)/$(ONE_IDTOOL_NAME)
LWIP_LIB           = $(BUILD)/$(LWIP_LIB_NAME)
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
SDK_INTERCEPT_C_FILES:=sockets.c \
						intercept.c \
						rpc.c


# Automagically pick clang or gcc, with preference for clang
# This is only done if we have not overridden these with an environment or CLI variable
ifeq ($(origin CC),default)
	CC=$(shell if [ -e /usr/bin/clang ]; then echo clang; else echo gcc; fi)
endif
ifeq ($(origin CXX),default)
	CXX=$(shell if [ -e /usr/bin/clang++ ]; then echo clang++; else echo g++; fi)
endif

INCLUDES=
DEFS=
LIBS=
ARCH_FLAGS=-arch x86_64
CFLAGS=

include objects.mk

# Disable codesign since open source users will not have ZeroTier's certs
CODESIGN=echo
PRODUCTSIGN=echo
CODESIGN_APP_CERT=
CODESIGN_INSTALLER_CERT=

# Debug output for ZeroTier service
ifeq ($(ZT_DEBUG),1)
	DEFS+=-DZT_TRACE
	CFLAGS+=-Wall -g -pthread $(INCLUDES) $(DEFS)
	STRIP=echo
	# The following line enables optimization for the crypto code, since
	# C25519 in particular is almost UNUSABLE in heavy testing without it.
#ext/lz4/lz4.o node/Salsa20.o node/SHA512.o node/C25519.o node/Poly1305.o: CFLAGS = -Wall -O2 -g -pthread $(INCLUDES) $(DEFS)
else
	CFLAGS?=-Ofast -fstack-protector
	CFLAGS+=$(ARCH_FLAGS) -Wall -flto -fPIE -pthread -mmacosx-version-min=10.7 -DNDEBUG -Wno-unused-private-field $(INCLUDES) $(DEFS)
	STRIP=strip
endif

# Debug output for the SDK
# Specific levels can be controlled in src/debug.h
ifeq ($(SDK_DEBUG),1)
	DEFS+=-DSDK_DEBUG -g
endif

CXXFLAGS=$(CFLAGS) -fno-rtti

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


# ------ MISC TARGETS ------
# Build everything
all: one osx ios android lwip check



# --- EXTERNAL LIBRARIES ---
lwip:
	-make -f make-liblwip.mk $(LWIP_FLAGS)

pico:
	mkdir -p build
	cd ext/picotcp; make lib ARCH=shared IPV4=1 IPV6=1
	$(CC) -g -nostartfiles -shared -o ext/picotcp/build/lib/libpicotcp.so ext/picotcp/build/lib/*.o ext/picotcp/build/modules/*.o
	cp ext/picotcp/build/lib/libpicotcp.so build/libpicotcp.so

jip:
	-make -f make-jip.mk $(JIP_FLAGS)

# ------- IOS / OSX --------
# Build all Apple targets
apple: one osx ios


# Build vanilla ZeroTier One binary
one: $(OBJS) $(ZT1)/service/OneService.o $(ZT1)/one.o $(ZT1)/osdep/OSXEthernetTap.o
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(BUILD)/zerotier-one $(OBJS) $(ZT1)/service/OneService.o $(ZT1)/one.o $(ZT1)/osdep/OSXEthernetTap.o $(LDLIBS)
	$(STRIP) $(ONE_SERVICE)
	cp $(ONE_SERVICE) $(INT)/docker/docker_demo/$(ONE_SERVICE_NAME)

# Build all iOS targets
ios: ios_app_framework ios_unity3d_bundle


# Build all OSX targets
osx: osx_app_framework osx_unity3d_bundle osx_shared_lib osx_sdk_service osx_intercept


# TODO: CHECK if XCODE TOOLS are installed
# Build frameworks for application development
osx_app_framework:
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Release -scheme ZeroTierSDK_OSX build SYMROOT="../../../$(BUILD)/osx_app_framework"
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Debug -scheme ZeroTierSDK_OSX build SYMROOT="../../../$(BUILD)/osx_app_framework"
	cp docs/osx_zt_sdk.md $(BUILD)/osx_app_framework/README.md
ios_app_framework:
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Release -scheme ZeroTierSDK_iOS build SYMROOT="../../../$(BUILD)/ios_app_framework"
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Debug -scheme ZeroTierSDK_iOS build SYMROOT="../../../$(BUILD)/ios_app_framework"
	cp docs/ios_zt_sdk.md $(BUILD)/ios_app_framework/README.md


# Build bundles for Unity integrations
osx_unity3d_bundle:
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Release -scheme ZeroTierSDK_Unity3D_OSX build SYMROOT="../../../$(BUILD)/osx_unity3d_bundle"
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Debug -scheme ZeroTierSDK_Unity3D_OSX build SYMROOT="../../../$(BUILD)/osx_unity3d_bundle"
	cp docs/osx_unity3d_zt_sdk.md $(BUILD)/osx_unity3d_bundle/README.md
	chmod 755 $(BUILD)/osx_unity3d_bundle/Debug/ZeroTierSDK_Unity3D_OSX.bundle
	cp -p -R $(BUILD)/osx_unity3d_bundle/Debug/ZeroTierSDK_Unity3D_OSX.bundle $(INT)/Unity3D/Assets/Plugins


ios_unity3d_bundle:
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Release -scheme ZeroTierSDK_Unity3D_iOS build SYMROOT="../../../$(BUILD)/ios_unity3d_bundle"
	cd $(INT)/apple/ZeroTierSDK_Apple; xcodebuild -configuration Debug -scheme ZeroTierSDK_Unity3D_iOS build SYMROOT="../../../$(BUILD)/ios_unity3d_bundle"
	cp docs/ios_unity3d_zt_sdk.md $(BUILD)/ios_unity3d_bundle/README.md

# 
simple_app: tests osx_service_and_intercept
	mkdir -p build/simple_app
	mkdir -p build/tests/zerotier
	$(CXX) -Isrc $(SHARED_LIB) integrations/simple_app/app.cpp -o $(BUILD)/simple_app/app
	cp $(LWIP_LIB) $(BUILD)/tests/zerotier/$(LWIP_LIB_NAME)


remove_only_intermediates:
	-find . -type f \( -name '*.o' -o -name '*.so' \) -delete


# Builds a single shared library which contains everything
ifeq ($(SDK_LWIP),1)
osx_shared_lib: lwip $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(STACK_FLAGS) -DSDK -DZT_ONE_NO_ROOT_CHECK $(INCLUDES) -DSDK_INTERCEPT -shared -o $(SHARED_LIB) $(OBJS) $(LWIP_DRIVER_FILES) $(SDK_SERVICE_CPP_FILES) $(SDK_SERVICE_C_FILES) $(LDLIBS) -ldl
else
osx_shared_lib: pico $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(STACK_FLAGS) -DSDK -DZT_ONE_NO_ROOT_CHECK $(INCLUDES) -shared -o $(SHARED_LIB) $(OBJS) $(PICO_DRIVER_FILES) $(SDK_SERVICE_CPP_FILES) $(SDK_SERVICE_C_FILES) $(LDLIBS) -ldl
endif

osx_intercept:
	# Use gcc not clang to build standalone intercept library since gcc is typically used for libc and we want to ensure maximal ABI compatibility
	cd src ; gcc $(DEFS) $(INCLUDES) -g -O2 -Wall -std=c99 -fPIC -DVERBOSE -D_GNU_SOURCE -DSDK_INTERCEPT -nostdlib -nostdlib -shared -o ../$(INTERCEPT) $(SDK_INTERCEPT_C_FILES) -ldl


# Build only the SDK service
ifeq ($(SDK_LWIP),1)
osx_sdk_service: lwip $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(STACK_FLAGS) $(DEFS) $(INCLUDES) -DSDK_SERVICE -DSDK -DZT_ONE_NO_ROOT_CHECK -o $(SDK_SERVICE) $(OBJS) $(LWIP_DRIVER_FILES) $(SDK_SERVICE_CPP_FILES) $(SDK_SERVICE_C_FILES) $(LDLIBS) -ldl
else
osx_sdk_service: pico $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(STACK_FLAGS) $(DEFS) $(INCLUDES) -DSDK_SERVICE -DSDK -DZT_ONE_NO_ROOT_CHECK -o $(SDK_SERVICE) $(OBJS) $(PICO_DRIVER_FILES) $(SDK_SERVICE_CPP_FILES) $(SDK_SERVICE_C_FILES) $(LDLIBS) -ldl
endif
	ln -sf $(SDK_SERVICE_NAME) $(BUILD)/zerotier-cli
	ln -sf $(SDK_SERVICE_NAME) $(BUILD)/zerotier-idtool


# Build both intercept library and SDK service (separate)
osx_service_and_intercept: osx_intercept osx_sdk_service



# -------- ANDROID ---------
# Build all Android targets
# Chip architectures can be specified in integrations/android/android_jni_lib/java/jni/Application.mk
android: android_jni_lib

# TODO: CHECK if ANDROID/GRADLE TOOLS are installed
# Build library for Android Unity integrations
# Build JNI library for Android app integration
android_jni_lib:
	-./increment.sh
	cd $(INT)/android/android_jni_lib/proj; ./gradlew assembleDebug
	mkdir -p $(BUILD)/android_jni_lib
	cp docs/android_zt_sdk.md $(BUILD)/android_jni_lib/README.md
	mv -f $(INT)/android/android_jni_lib/java/libs/* $(BUILD)/android_jni_lib
	cp -R $(BUILD)/android_jni_lib/* $(INT)/android/example_app/app/src/main/jniLibs




# -------- TESTING ---------
# Check for the presence of built frameworks/bundles/libaries
check:
	./check.sh $(LWIP_LIB)
	./check.sh $(INTERCEPT)
	./check.sh $(ONE_SERVICE)
	./check.sh $(SDK_SERVICE)
	./check.sh $(SHARED_LIB)
	./check.sh $(BUILD)/osx_unity3d_bundle/Debug/ZeroTierSDK_Unity3D_OSX.bundle
	./check.sh $(BUILD)/osx_app_framework/Debug/ZeroTierSDK_OSX.framework
	./check.sh $(BUILD)/ios_app_framework/Debug-iphoneos/ZeroTierSDK_iOS.framework
	./check.sh $(BUILD)/ios_unity3d_bundle/Debug-iphoneos/ZeroTierSDK_Unity3D_iOS.bundle
	./check.sh $(BUILD)/android_jni_lib/arm64-v8a/libZeroTierOneJNI.so
	./check.sh $(BUILD)/android_jni_lib/armeabi/libZeroTierOneJNI.so
	./check.sh $(BUILD)/android_jni_lib/armeabi-v7a/libZeroTierOneJNI.so
	./check.sh $(BUILD)/android_jni_lib/mips/libZeroTierOneJNI.so
	./check.sh $(BUILD)/android_jni_lib/mips64/libZeroTierOneJNI.so
	./check.sh $(BUILD)/android_jni_lib/x86/libZeroTierOneJNI.so
	./check.sh $(BUILD)/android_jni_lib/x86_64/libZeroTierOneJNI.so

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

test_suite: tests lwip osx_service_and_intercept
	cp tests/api_test/test.sh $(BUILD)/tests/test.sh
	cp tests/api_test/servers.sh $(BUILD)/tests/servers.sh
	cp tests/api_test/clients.sh $(BUILD)/tests/clients.sh
	cp tests/cleanup.sh $(BUILD)/tests/cleanup.sh
	cp $(LWIP_LIB) $(BUILD)/tests/zerotier/$(LWIP_LIB_NAME)


# ----- ADMINISTRATIVE -----
JAVAC := $(shell which javac)

clean_unity:
	
clean_android:
	# android JNI lib project
	-test -s /usr/bin/javac || { echo "Javac not found"; exit 1; }
	-cd $(INT)/android/android_jni_lib/proj; ./gradlew clean
	-rm -rf $(INT)/android/android_jni_lib/proj/build
	# example android app project
	-cd $(INT)/android/example_app; ./gradlew clean

clean_basic:
	#-rm -rf .depend
	-rm -rf $(BUILD)/*
	-rm -rf $(INT)/Unity3D/Assets/Plugins/*
	-rm -rf zerotier-cli zerotier-idtool
	-find . -type f \( -name $(ONE_SERVICE_NAME) -o -name $(SDK_SERVICE_NAME) \) -delete
	-find . -type f \( -name '*.o' -o -name '*.so' -o -name '*.o.d' -o -name '*.out' -o -name '*.log' -o -name '*.dSYM' \) -delete

clean: clean_basic

clean_for_production:
	-find . -type f \( -name '*.identity'\) -delete

prep:
	cp $(INT)/android/android_jni_lib/java/libs/* build

# Copy and rename source files into example projects to follow local ordinances
update_examples:
	cp src/wrappers/dotnet/DotNetWrapper.cs integrations/Unity3D/Assets/ZTSDK.cs
	cp src/wrappers/java/JavaWrapper.java integrations/android/example_app/app/src/main/java/ZeroTier/ZTSDK.java

# For authors
# Copies documentation to all of the relevant directories to make viewing in the repo a little easier
update_docs:
	cp docs/android_zt_sdk.md integrations/android/README.md
	cp docs/ios_zt_sdk.md integrations/apple/example_app/iOS/README.md
	cp docs/osx_zt_sdk.md integrations/apple/example_app/OSX/README.md
	cp docs/integrations.md integrations/README.md
	cp docs/zt_sdk_intro.md README.md
	cp docs/docker_linux_zt_sdk.md integrations/docker/README.md
	cp docs/osx_unity3d_zt_sdk.md integrations/Unity3D/README.md
