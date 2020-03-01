#! /bin/bash
# First parameter is version number - (ex: 1.0.0)
# Second parametr is build or release status - (ex: test, beta, or release)
# Third parameter is optional
#

if [ -z "$1" ]; then
  echo "1st parameter must be the version number (x.y.z)"
  exit
fi

if [ "$2" != "test" ] && [ "$2" != "beta" ] && [ "$2" != "release" ]; then
  echo "2nd parameter must be 'test' or 'beta' or 'release'."
  exit
fi

# application name
appli_name="OSCAR"
pre_inst="tst_user.sh"
post_inst="ln_usrbin.sh"
pre_rem="rm_usrbin.sh"
post_rem="clean_rm.sh"
iter=""
# build folder (absolute path is better)
build_folder="/home/$USER/OSCAR/build"
if [ "$2" != "release" ]; then
    appli_name=${appli_name}-test
    post_inst="ln_usrbin-test.sh"
    pre_rem="rm_usrbin-test.sh"
    post_rem="clean_rm-test.sh"
    iter="$2"
fi
if [ -n "$3" ]; then
    iter="$iter"~"$3"
fi
# temporary folder (absolute path is better)
temp_folder="/home/$USER/tmp_Ubu_${appli_name}/"

# destination folder in the .deb file
dest_folder="/usr/"

# the .deb file mustn't exist (or fpm must have -f parameter to force the creation)
archi_tmp=$(lscpu | grep -i architecture | awk -F: {'print $2'} | tr -d " ")
if [ "$archi_tmp" = "x86_64" ];then
    archi="amd64"
else
    archi="unknown"
fi
deb_file="${appli_name}_$1-$2_$archi.deb"

# if deb file exists, fatal error
if [ -f "./$deb_file" ]; then
    echo "destination file (./$deb_file) exists. fatal error"
    exit 
fi

# clean folders need to create the package
if [ -d "${temp_folder}" ]; then
    rm -r ${temp_folder}
fi
mkdir ${temp_folder}
if [ ! -d "${temp_folder}" ]; then
    echo "Folder (${temp_folder}) not created : fatal error."
    exit
fi
chmod 0755 ${temp_folder}
# save current value of umask (for u=g and not g=o)
current_value=$(umask)
umask 022
mkdir ${temp_folder}/bin
mkdir ${temp_folder}/share
mkdir ${temp_folder}/share/${appli_name}
mkdir ${temp_folder}/share/doc
share_doc_folder="${temp_folder}/share/doc/${appli_name}"
mkdir ${share_doc_folder}
mkdir ${temp_folder}/share/icons
mkdir ${temp_folder}/share/icons/hicolor
mkdir ${temp_folder}/share/icons/hicolor/48x48
mkdir ${temp_folder}/share/icons/hicolor/48x48/apps
mkdir ${temp_folder}/share/icons/hicolor/scalable
mkdir ${temp_folder}/share/icons/hicolor/scalable/apps
mkdir ${temp_folder}/share/applications

# must delete debug symbol in OSCAR binary file
# --- V1
strip -s -o ${temp_folder}/bin/${appli_name} ${build_folder}/oscar/OSCAR
#old code : cp ${build_folder}/oscar/OSCAR ${temp_folder}/bin

# 2>/dev/null : errors does not appear : we don't care about them
cp -r ${build_folder}/oscar/Help ${temp_folder}/share/${appli_name} 2>/dev/null
cp -r ${build_folder}/oscar/Html ${temp_folder}/share/${appli_name} 2>/dev/null
cp -r ${build_folder}/oscar/Translations ${temp_folder}/share/${appli_name} 2>/dev/null
cp ./${appli_name}.png ${temp_folder}/share/icons/hicolor/48x48/apps/${appli_name}.png
cp ./${appli_name}.svg ${temp_folder}/share/icons/hicolor/scalable/apps/${appli_name}.svg
cp ./${appli_name}.desktop ${temp_folder}/share/applications/${appli_name}.desktop

echo "Copyright 2019-2020 team-oscar.org <oscar@team-oscar.org>" > $share_doc_folder/copyright

changelog_file="$share_doc_folder/changelog"

#automatic changelog as a bad name
# need to generate one and say fpm to use it instead of create one
# it seems that it needs both of them...

# creation of the changelog.Debian.gz
echo "$appli_name ($1-$2) whatever; urgency=medium" > $changelog_file
echo "" >> $changelog_file
echo "  * Package created with FPM." >> $changelog_file
echo "" >> $changelog_file
echo " -- team-oscar.org <oscar@team-oscar.org>" >> $changelog_file
gzip --best $changelog_file
description='Open Source CPAP Analysis Reporter\n<extended description needed to be filled with the right value>'
# trick for dummies : need to use echo -e to take care of \n (cariage return to slip description and extra one
description=$(echo -e $description)

# restore umask value
umask $current_value

# create the .deb file (litian test show juste a warning with a man that doesn't exists : don't care about that)
fpm --input-type dir --output-type deb  \
    --prefix ${dest_folder} \
    --before-install ${pre_inst} \
    --after-install ${post_inst}   \
    --before-remove ${pre_rem}   \
    --after-remove ${post_rem} \
    --name ${appli_name} --version $1 --iteration $iter \
    --category misc               \
    --deb-priority optional \
    --maintainer " -- team-oscar.org <oscar@team-oscar.org>"   \
    --license GPL-v3                \
    --vendor team-oscar.org    \
    --description "$description" \
    --url https://sleepfiles.com/OSCAR  \
    --deb-no-default-config-files   \
    --depends libdouble-conversion1 \
    --depends libpcre16-3 \
    --depends qttranslations5-l10n \
    --depends 'libqt5core5a > 5.9.0'    \
    --depends libqt5serialport5     \
    --depends libqt5xml5            \
    --depends libqt5network5        \
    --depends libqt5gui5            \
    --depends libqt5widgets5        \
    --depends libqt5opengl5         \
    --depends libqt5printsupport5   \
    --depends libglu1-mesa          \
    --depends libgl1                \
    --depends libc6                 \
    --no-deb-generate-changes \
    -C ${temp_folder} \
    .

