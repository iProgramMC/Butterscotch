#!/bin/bash

#make OS=iOS CFLAGS="-target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk -ObjC" LDFLAGS="-target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk" CC="$THEOS/toolchain/linux/iphone/bin/clang" LD="$THEOS/toolchain/linux/iphone/bin/clang"
make -f Makefile_iOS.mk OS=iOS DISABLE_MODERN_GL=1 CFLAGS="-target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk -ObjC -DIPHONE_OS_3" LDFLAGS="-target armv7-apple-darwin9 -isysroot $THEOS/sdks/iPhoneOS3.1.3.sdk" CC="$THEOS/toolchain/linux/iphone/bin/clang" LD="$THEOS/toolchain/linux/iphone/bin/clang"

#sign that shi
CODESIGN_ALLOCATE=$THEOS/toolchain/linux/iphone/bin/codesign_allocate $THEOS/toolchain/linux/iphone/bin/ldid -Sbutterscotch.entitlements build/butterscotch

