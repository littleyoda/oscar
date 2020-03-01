#! /bin/bash
# First parameter is version number - (ex: 1.0.0)
# Second parametr is build or release status - (ex: beta or release)
#

# application name
appli_name="OSCAR"
# build folder (absolute path is better)
build_folder="/home/$USER/OSCAR/build"
# temporary folder (absolute path is better)
temp_folder="/home/$USER/tmp_RH_${appli_name}/"
chmod 775 $temp_folder

# destination folder in the .rpm file
dest_folder="/usr/"

if [ -z "$1" ]; then
  echo "1st parameter must be the version number (x.y.z)"
  exit
fi

if [ "$2" != "beta" ] && [ "$2" != "release" ]; then
  echo "2nd parameter must 'beta' or 'release'."
  exit
fi

# the .rpm file mustn't exist (or fpm must have -f parameter to force the creation)
archi=$(lscpu | grep -i architecture | awk -F: {'print $2'} | tr -d " ")
rpm_file="${appli_name}-$1-$2.$archi.rpm"

# if rpm file exists, fatal error
if [ -f "./$rpm_file" ]; then
    echo "destination file (./$rpm_file) exists. fatal error"
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
mkdir ${temp_folder}/share/icons/${appli_name}
mkdir ${temp_folder}/share/applications

# must delete debug symbol in OSCAR binary file
# -- V1 :
strip -s -o ${temp_folder}/bin/${appli_name} ${build_folder}/oscar/OSCAR
#old code : cp ${build_folder}/oscar/OSCAR ${temp_folder}/bin

# for QT5.7
# strip -s -o ${temp_folder}/bin/oscar.qt5.7 ./oscar.qt5.7
# for QT5.9
# strip -s -o ${temp_folder}/bin/oscar.qt5.9 ./oscar.qt5.9

# 2>/dev/null : errors does not appear : we don't care about them
cp -r ${build_folder}/oscar/Help ${temp_folder}/share/${appli_name} 2>/dev/null
cp -r ${build_folder}/oscar/Html ${temp_folder}/share/${appli_name} 2>/dev/null
cp -r ${build_folder}/oscar/Translations ${temp_folder}/share/${appli_name} 2>/dev/null
cp ./OSCAR.png ${temp_folder}/share/icons/${appli_name}/${appli_name}.png
cp ./OSCAR.desktop ${temp_folder}/share/applications/${appli_name}.desktop

echo "Copyright 20192020 team-oscar.org <oscar@team-oscar.org>" > $share_doc_folder/copyright

changelog_file="$share_doc_folder/changelog"

#automatic changelog as a bad name
# need to generate one and say fpm to use it instead of create one
# it seems that it needs both of them...

# creation of the changelog.Debian.gz (for rpm package ? )
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

# create the .rpm file (litian test show juste a warning with a man that doesn't exists : don't care about that)
# pour 32bit : i686 au lieu de x86_64
fpm --input-type dir --output-type rpm  \
    --prefix ${dest_folder} \
    --after-install ln_usrbin.sh   \
    --before-remove rm_usrbin.sh   \
    --after-remove clean_rm.sh \
    --name ${appli_name} --version $1 --iteration $2 \
    --category misc               \
    --maintainer " -- team-oscar.org <oscar@team-oscar.org>"   \
    --license GPL-v3                \
    --vendor team-oscar.org    \
    --description "$description" \
    --url https://sleepfiles.com/OSCAR  \
    -C ${temp_folder} \
    .


