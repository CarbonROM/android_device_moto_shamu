# Copyright 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Kernel
BOARD_KERNEL_IMAGE_NAME := zImage-dtb
TARGET_KERNEL_SOURCE := kernel/moto/msm
TARGET_KERNEL_CONFIG := shamu_defconfig

# Inherit AOSP Shinano common device parts
$(call inherit-product, device/moto/shamu/aosp_shamu.mk)

# Inherit Carbon GSM telephony parts
$(call inherit-product, vendor/carbon/config/gsm.mk)

# Inherit Carbon product configuration
$(call inherit-product, vendor/carbon/config/common.mk)

# Override Product Name for CarbonROM
PRODUCT_NAME := carbon_shamu
PRODUCT_BRAND := google
PRODUCT_MODEL := Nexus 6
PRODUCT_RESTRICT_VENDOR_FILES := false

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME=shamu \
    BUILD_FINGERPRINT="google/shamu/shamu:6.0/MRA58K/2256973:user/release-keys" \
    PRIVATE_BUILD_DESC="shamu-user 6.0 MRA58K 2256973 release-keys"