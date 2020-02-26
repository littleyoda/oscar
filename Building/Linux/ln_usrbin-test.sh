#! /bin/bash
set -e
#
# modify by untoutseul05 to search local name for Desktop Folder
# the package now suits the fhs 

# application name
appli_name="OSCAR-test"

# binary copy flag
copy_flag=0

# Select the binary file for the goog version of QT (5.7 ou 5.9)
#echo "test the version of qt5 core version"

if [ -f "/etc/redhat-release" ]; then
    # pour mageia (red hat)
    echo "distribution : mageia (red hat)"
    Qt5_core=$(yum search -q qt5core5 | awk '{print $1}' | grep lib | sort -u)
    MajorVer_no=$(yum info $Qt5_core | grep -i "version" | awk '{print $3}' | awk -F. '{print $1}')
    MinorVer_no=$(yum info $Qt5_core | grep -i "version" | awk '{print $3}' | awk -F. '{print $2}')

elif [ -f "/etc/lsb-release" ] || [ -f "/etc/debian_version" ]; then
    # pour debian
#    echo "distribution : debian"
    Qt5_core=$(dpkg -l | awk '{print $2}'| grep qt5 | grep core | awk -F: '{print $1}')
    Major_ver=$(dpkg -l | grep $Qt5_core | awk '{print $3}' | awk -F. '{print $1}')
    Minor_ver=$(dpkg -l | grep $Qt5_core | awk '{print $3}' | awk -F. '{print $2}')
else
   echo "unknown distribution "
   exit
fi

#echo "Qt5_core  = '$Qt5_core'"
#echo "Major_ver = $Major_ver"
#echo "Minor_ver = $Minor_ver"

if [ -x /usr/bin/update-menus ]; then update-menus; fi

if [ ! -z "$SUDO_USER" ]; then
    # find real name of the Desktop folder (Bureau for xubuntu french version)
    desktop_folder_name="/home/$SUDO_USER/Desktop"
    
    # si doesn't exist, try to find it translated name 
    tmp_dir="" 
    if [ ! -d "$desktop_folder_name" ]; then
        tmp_dir=`cat /home/$SUDO_USER/.config/user-dirs.dirs | grep XDG_DESKTOP_DIR | awk -F= '{print $2}' | awk -F\" '{print $2}' | awk -F\/ '{print $2}'`
    fi
    
    # don't overwrite if translated name or doesn't exist
    if [ -n "$tmp_dir" ];  then
        # calculate the full folder
        tmp_dir_full="/home/${SUDO_USER}/${tmp_dir}"
        if [ -d "$tmp_dir_full" ]; then
            desktop_folder_name=$tmp_dir_full
        fi
    fi
    
    # info : /usr/share/applications/${appli_name}.desktop
    # copy icon file to the Desktop folder (even if it has been translated)
    file_from="/usr/share/applications/${appli_name}.desktop"
    file_to="$desktop_folder_name/${appli_name}.desktop"
    
    cp $file_from $file_to
    chown $SUDO_USER:$SUDO_USER $file_to
    chmod a+x $file_to
fi
