#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Build both ARMv5TE and ARMv7-A machine code.

APP_PLATFORM := android-9

ifeq ($(PALTFORM_DIR),armv6)
APP_ABI := armeabi      #armeabi armeabi-v7a x86
endif
ifeq ($(PALTFORM_DIR),vfp)
APP_ABI := armeabi      #armeabi armeabi-v7a x86
endif
ifeq ($(PALTFORM_DIR),armv7)
APP_ABI := armeabi-v7a      #armeabi armeabi-v7a x86
endif
ifeq ($(PALTFORM_DIR),neon)
APP_ABI := armeabi-v7a      #armeabi armeabi-v7a x86
endif
ifeq ($(PALTFORM_DIR),x86)
APP_ABI := x86      #armeabi armeabi-v7a x86
endif
ifeq ($(PALTFORM_DIR),mips)
APP_ABI := mips      #armeabi armeabi-v7a x86
endif

APP_STL := gnustl_static #stlport_static #gnustl_static #stlport_static
