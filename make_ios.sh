#!/bin/bash

#make OS=iOS CFLAGS="-target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk -ObjC" LDFLAGS="-target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk" CC="$THEOS/toolchain/linux/iphone/bin/clang" LD="$THEOS/toolchain/linux/iphone/bin/clang"
make \
	-f Makefile_iOS.mk \
	OS=iOS \
	DISABLE_MODERN_GL=1 \
	CFLAGS="-ObjC -DIPHONE_OS_3 -O3 -DNDEBUG -flto" \
	CC="$THEOS/toolchain/linux/iphone/bin/clang -target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk" \
	LD="$THEOS/toolchain/linux/iphone/bin/clang -target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk"

#sign that shi
CODESIGN_ALLOCATE=$THEOS/toolchain/linux/iphone/bin/codesign_allocate $THEOS/toolchain/linux/iphone/bin/ldid -Sbutterscotch.entitlements build/butterscotch
