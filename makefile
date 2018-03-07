TARGET_PLATFORM=android-27
BUILD_TOOLS_VERSION=27.0.3

BUILD_TOOLS=$(ANDROID_HOME)/build-tools/$(BUILD_TOOLS_VERSION)
ANDROID_SDK_PLATFORM=$(ANDROID_HOME)/platforms/$(TARGET_PLATFORM)
ANDROID_NDK_PLATFORM=$(ANDROID_NDK)/platforms/$(TARGET_PLATFORM)
ANDROID_NDK_INCLUDES=$(ANDROID_NDK)/sysroot/usr/include
CC=$(ANDROID_NDK)/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc

all: andrecord.apk pamnc

andrecord.apk: keystore.jks build/andrecord.aligned.apk
	$(BUILD_TOOLS)/apksigner sign --ks keystore.jks --ks-key-alias androidkey \
		--ks-pass pass:android --key-pass pass:android --out andrecord.apk \
		build/andrecord.aligned.apk

keystore.jks:
	keytool -genkeypair -keystore keystore.jks -alias androidkey -validity 10000 \
		-keyalg RSA -keysize 2048 -storepass android -keypass android

build/andrecord.aligned.apk: build/andrecord.unsigned.apk
	$(BUILD_TOOLS)/zipalign -f 4 build/andrecord.unsigned.apk \
		build/andrecord.aligned.apk

build/andrecord.unsigned.apk: build AndroidManifest.xml apk/lib/armeabi-v7a/libmain.so
	$(BUILD_TOOLS)/aapt package -f -M AndroidManifest.xml \
		-I $(ANDROID_SDK_PLATFORM)/android.jar -F build/andrecord.unsigned.apk \
		apk

build:
	mkdir build

apk/lib/armeabi-v7a/libmain.so: apk/lib/armeabi-v7a obj/andrecord.o obj/jhelpers.o obj/bufqueue.o obj/sles.o
	$(CC) -O3 -s -shared -fvisibility=hidden \
		obj/andrecord.o obj/jhelpers.o obj/bufqueue.o obj/sles.o \
		--sysroot $(ANDROID_NDK_PLATFORM)/arch-arm -llog -lOpenSLES \
		-o apk/lib/armeabi-v7a/libmain.so

apk/lib/armeabi-v7a:
	mkdir -p apk/lib/armeabi-v7a

obj/andrecord.o: obj andrecord.c jhelpers.h bufqueue.h sles.h utils.h makefile
	$(CC) -std=gnu11 -Wall -Wextra -pedantic -O3  -fvisibility=hidden \
		-c andrecord.c -o obj/andrecord.o -I$(ANDROID_NDK_INCLUDES) \
		-I$(ANDROID_NDK_INCLUDES)/arm-linux-androideabi

obj/jhelpers.o: obj jhelpers.c jhelpers.h utils.h makefile
	$(CC) -std=gnu11 -Wall -Wextra -pedantic -O3  -fvisibility=hidden \
		-c jhelpers.c -o obj/jhelpers.o -I$(ANDROID_NDK_INCLUDES) \
		-I$(ANDROID_NDK_INCLUDES)/arm-linux-androideabi

obj/bufqueue.o: obj bufqueue.c bufqueue.h utils.h makefile
	$(CC) -std=gnu11 -Wall -Wextra -pedantic -O3  -fvisibility=hidden \
		-c bufqueue.c -o obj/bufqueue.o -I$(ANDROID_NDK_INCLUDES) \
		-I$(ANDROID_NDK_INCLUDES)/arm-linux-androideabi

obj/sles.o: obj sles.c sles.h jhelpers.h utils.h makefile
	$(CC) -std=gnu11 -Wall -Wextra -pedantic -O3  -fvisibility=hidden \
		-c sles.c -o obj/sles.o -I$(ANDROID_NDK_INCLUDES) \
		-I$(ANDROID_NDK_INCLUDES)/arm-linux-androideabi

obj:
	mkdir obj

pamnc: pamnc.c makefile
	gcc -std=gnu11 -Wall -Wextra -pedantic -O3 -s pamnc.c -o pamnc

clean:
	rm -rf andrecord.apk build apk obj pamnc
