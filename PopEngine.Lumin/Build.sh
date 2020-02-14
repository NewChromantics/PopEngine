#!/bin/sh
#env

if [ -z "$PROJECT_NAME" ]; then
	echo "PROJECT_NAME env var not set (expected PopEngine)"
	exit 1
fi

if [ -z "$MABU_BUILD_TARGET" ]; then
	echo "MABU_BUILD_TARGET env var not set (expected ml1 or release_ml1)"
	exit 1
fi

# build dir used by xcode
if [ -z "$BUILD_ROOT" ]; then
	echo "BUILD_ROOT env var not set (expected .so's build directory)"
	exit 1
fi

if [ -z "$MABU_FILENAME" ]; then
	#echo "MABU_FILENAME env var not set (expected ...)"
	#exit 1
	MABU_FILENAME="$PROJECT_NAME.Lumin/$PROJECT_NAME.mabu"
fi

# useful output directory, eg, unity dir
if [ -z "$BUILD_OUTPUT_PATH" ]; then
	#echo "MABU_FILENAME env var not set (expected ...)"
	#exit 1
	BUILD_OUTPUT_PATH="$PROJECT_NAME.Lumin/Build/$MABU_BUILD_TARGET/"
fi

if [ -z "$MAGIC_LEAP_SDK_PATH" ]; then
	echo "MAGIC_LEAP_SDK_PATH env var not set (expected /Volumes/Assets/mlsdk/v0.23.0)"
	exit 1
fi


# https://developer.magicleap.com/learn/guides/getting-started-in-magic-leap-builder
# -b build
# -c clean
# -r rebuild
# -p package
MABU_KIND=Shared
export MABU_KIND=Shared
echo "SOURCE_ROOT = $SOURCE_ROOT"
$MAGIC_LEAP_SDK_PATH/mabu -b  -t $MABU_BUILD_TARGET --out $BUILD_ROOT MABU_KIND=Shared $MABU_FILENAME --print-build-vars

RESULT=$?

if [[ $RESULT -ne 0 ]]; then
	exit $RESULT
fi

SRC_PATH="$BUILD_ROOT/release_lumin_clang-3.8_aarch64/lib$PROJECT_NAME.so"
echo "Copying $SRC_PATH to $BUILD_OUTPUT_PATH"

mkdir -p $BUILD_OUTPUT_PATH && cp $SRC_PATH $BUILD_OUTPUT_PATH

RESULT=$?
if [[ $RESULT -ne 0 ]]; then
	exit $RESULT
fi

exit 0
