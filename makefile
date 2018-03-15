TARGET_PLATFORM := android-27
BUILD_TOOLS_VERSION := 27.0.3

BUILD_TOOLS := $(ANDROID_HOME)/build-tools/$(BUILD_TOOLS_VERSION)
ANDROID_SDK_PLATFORM := $(ANDROID_HOME)/platforms/$(TARGET_PLATFORM)
ANDROID_NDK_PLATFORM := $(ANDROID_NDK)/platforms/$(TARGET_PLATFORM)
ANDROID_NDK_INCLUDES := $(ANDROID_NDK)/sysroot/usr/include
TOOLCHAIN := $(ANDROID_NDK)/toolchains/arm-linux-androideabi-4.9

CC := $(TOOLCHAIN)/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc
CFLAGS := -std=gnu11 -Wall -Wextra -pedantic -O3 -fvisibility=hidden \
	-I$(ANDROID_NDK_INCLUDES) -I$(ANDROID_NDK_INCLUDES)/arm-linux-androideabi
LDFLAGS := -O3 -s -shared -fvisibility=hidden -llog -lOpenSLES \
	--sysroot $(ANDROID_NDK_PLATFORM)/arch-arm

sources := $(filter-out pamnc.c,$(wildcard *.c))
objects := $(patsubst %.c,obj/%.o,$(sources))

all: andrecord.apk pamnc

andrecord.apk: keystore.jks build/andrecord.aligned.apk
	$(BUILD_TOOLS)/apksigner sign --ks keystore.jks --ks-key-alias androidkey \
		--ks-pass pass:android --key-pass pass:android --out andrecord.apk \
		build/andrecord.aligned.apk

keystore.jks:
	keytool -genkeypair -keystore keystore.jks -alias androidkey \
		-validity 10000 -keyalg RSA -keysize 2048 -storepass android \
		-keypass android

build/andrecord.aligned.apk: build/andrecord.unsigned.apk
	$(BUILD_TOOLS)/zipalign -f 4 build/andrecord.unsigned.apk \
		build/andrecord.aligned.apk

build/andrecord.unsigned.apk: AndroidManifest.xml apk/lib/armeabi-v7a/libmain.so
	mkdir -p $(dir $@)
	$(BUILD_TOOLS)/aapt package -f -M AndroidManifest.xml \
		-I $(ANDROID_SDK_PLATFORM)/android.jar -F build/andrecord.unsigned.apk \
		apk

apk/lib/armeabi-v7a/libmain.so: $(objects)
	mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@

obj/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

pamnc: pamnc.c
	gcc -std=gnu11 -Wall -Wextra -pedantic -O3 -s pamnc.c -o pamnc

clean:
	rm -rf andrecord.apk build apk obj pamnc
