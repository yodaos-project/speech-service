local-path := $(call my-dir)

include $(clear_vars)
local.module := speech-service
local.ndk-modules := speech-service.ndk
include $(build-executable)

include $(clear-vars)
local.module := speech-service.ndk
local.ndk-script := $(local-path)/ndk.mk
local.ndk-modules := flora-cli speech
include $(build-ndk-module)
