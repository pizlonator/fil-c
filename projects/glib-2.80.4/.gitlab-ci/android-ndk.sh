#!/bin/bash

#
# Copyright 2022 Collabora ltd.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: Xavier Claessens <xavier.claessens@collabora.com>
#

set -e

# Download Android NDK
ANDROID_NDK_PATH=/opt/android-ndk
ANDROID_NDK_VERSION="r23b"
ANDROID_NDK_SHA512="5f2b58e605fc99d4fd3e9d2210e7f5e76e89245fa9428ce0d890e2e03b598c62c48ebd528fcb76556f04b46b87afea52e1e8d280f32cd1232f290e074bfa56fa"
wget --quiet "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux.zip"
echo "${ANDROID_NDK_SHA512}  android-ndk-${ANDROID_NDK_VERSION}-linux.zip" | sha512sum -c
unzip "android-ndk-${ANDROID_NDK_VERSION}-linux.zip"
rm "android-ndk-${ANDROID_NDK_VERSION}-linux.zip"
mv "android-ndk-${ANDROID_NDK_VERSION}" "${ANDROID_NDK_PATH}"
