#!/bin/bash
set -euo pipefail
src_dir=$(cd -- "$(dirname -- "$0")/echo" && pwd)
build_dir=$(realpath -m -- "${1:?Usage: build_echo.sh OUTPUT_DIRECTORY}")
sdk_dir=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-${HOME}/Android/Sdk}}
android_jar="$sdk_dir/platforms/android-15/android.jar"
build_tools=${BUILD_TOOLS:-$sdk_dir/build-tools/36.0.0}
mkdir -p "$build_dir"/{classes,gen}
"$build_tools/aapt" package -f -m -J "$build_dir/gen" -M "$src_dir/AndroidManifest.xml" -S "$src_dir/res" -I "$android_jar" -F "$build_dir/echo-unsigned.apk"
"${JAVAC:-javac}" -source 8 -target 8 -bootclasspath "$android_jar" -d "$build_dir/classes" "$src_dir"/src/org/galaxyy/aoa/*.java
"$build_tools/d8" --min-api 15 --lib "$android_jar" --output "$build_dir" "$build_dir"/classes/org/galaxyy/aoa/*.class
(cd "$build_dir" && zip -q echo-unsigned.apk classes.dex)
if [ ! -f "$build_dir/test.keystore" ]; then
    keytool -genkeypair -keystore "$build_dir/test.keystore" -storepass android -keypass android -alias aoa-test -keyalg RSA -keysize 2048 -validity 3650 -dname 'CN=Galaxy Y AOA Test' >/dev/null
fi
"$build_tools/apksigner" sign --ks "$build_dir/test.keystore" --ks-pass pass:android --key-pass pass:android --ks-key-alias aoa-test --min-sdk-version 15 --v1-signing-enabled true --out "$build_dir/aoa-echo.apk" "$build_dir/echo-unsigned.apk"
"$build_tools/apksigner" verify --min-sdk-version 15 "$build_dir/aoa-echo.apk"
