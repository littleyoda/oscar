#!/bin/bash
#
# To update the version of Botan included in OSCAR, simply run this script.
# To change which modules are included, modify the MODULES variable below.

MODULES="aes,gcm,sha2_32,pbkdf2"

BOTAN_DIR=$1
if [ -z "$BOTAN_DIR" ]; then
    echo "Usage: $0 PATH"
    echo "  PATH   Directory of the Botan distribution to use"
    exit 1
fi
# Convert to absolute path
BOTAN_DIR="$(cd "$(dirname "$BOTAN_DIR")"; pwd -P)/$(basename "$BOTAN_DIR")"


SCRIPT_DIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'tmp')
echo "Building in ${TMP_DIR}"
pushd "${TMP_DIR}" > /dev/null

declare -a PLATFORMS

generate() {
    label="$1"; shift
    cpu="$1"; shift
    os="$label"
    if [[ $# -gt 0 ]]; then
        os="$1"
    fi
    PLATFORMS+=($label)
    mkdir "${label}"
    pushd "${label}" > /dev/null
    echo "Generating ${label}:"
    "$BOTAN_DIR/configure.py" --amalgamation --os="$os" --cpu="$cpu" --disable-shared --minimized-build --enable-modules="$MODULES"
    popd > /dev/null
}

generate linux generic
generate macos generic
generate windows generic mingw

# Make sure all the cpp files are identical as expected.
for platform in "${PLATFORMS[@]}"; do
    cmp "${PLATFORMS[0]}/botan_all.cpp" "${platform}/botan_all.cpp" || exit 1
done

echo "Copying files..."
cp "${PLATFORMS[0]}/botan_all.cpp" "${SCRIPT_DIR}/botan_all.cpp" || exit 1

# Copy the platform-specific h files.
for platform in "${PLATFORMS[@]}"; do
    cp "${platform}/botan_all.h" "${SCRIPT_DIR}/botan_${platform}.h" || exit 1
done

for platform in "${PLATFORMS[@]}"; do
    rm -r "${platform}"
done

popd > /dev/null
rmdir ${TMP_DIR}
echo "Done."
