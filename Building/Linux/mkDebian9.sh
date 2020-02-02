#! /bin/bash
#
# First (optional) parameter is package version (ex: "1") 
#
# This generally should start at 1 for each VERSION and increment any time the
# package needs to be updated.
#
# see https://serverfault.com/questions/604541/debian-packages-version-convention

ITERATION=$1
[[ ${ITERATION} == ""] && ITERATION="1"

SRC=./OSCAR-code

VERSION=`awk '/#define VERSION / { gsub(/"/, "", $3); print $3 }' ${SRC}/VERSION
if [[ VERSION == *-* ]]; then
    # Use ~ for prerelease information so that it sorts correctly compared to release
    # versions. See https://www.debian.org/doc/debian-policy/ch-controlfields.html#version
    IFS="-" read -r VERSION PRERELEASE <<< ${VERSION}
    VERSION="${VERSION}~${PRERELEASE}"
fi
GIT_REVISION=`awk '/#define GIT_REVISION / { gsub(/"/, "", $3); print $3 }' ${SRC}/git_info.h`

rm -r tempDir
mkdir tempDir
cp build/oscar/OSCAR tempDir
cp -r build/oscar/Help tempDir
cp -r build/oscar/Html tempDir
cp -r build/oscar/Translations tempDir
cp OSCAR.png tempDir
cp OSCAR.desktop tempDir
#cp OSCAR-code/migration.sh tempDir
#
fpm --input-type dir --output-type deb  \
    --prefix /opt                   \
    --after-install ln_usrlocalbin.sh   \
    --before-remove rm_usrlocalbin.sh   \
    --name oscar --version ${VERSION} --iteration ${ITERATION} \
    --category Other                \
    --maintainer oscar@nightowlsoftwre.ca   \
    --license GPL-v3                \
    --vendor nightowlsoftware.ca    \
    --description "Open Sourece CPAP Analysis Reporter" \
    --url https://sleepfiles.com/OSCAR  \
    --depends 'libqt5core5a > 5.7.0'    \
    --depends libqt5serialport5     \
    --depends libqt5xml5            \
    --depends libqt5network5        \
    --depends libqt5gui5            \
    --depends libqt5widgets5        \
    --depends libqt5opengl5         \
    --depends libqt5printsupport5   \
    --depends libglu1-mesa          \
    --depends libgl1                \
    --deb-no-default-config-files   \
    tempDir
