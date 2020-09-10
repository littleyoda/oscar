#! /bin/bash
#
# no error is permitted
set -e

# application name
appli_name="OSCAR-test"

if [ ! -z "$SUDO_USER" ]; then
    # find real name of the Desktop folder (Bureau for xubuntu french version)
    desktop_folder_name="/home/$SUDO_USER/Desktop"
    
    # si doesn't exist, try to find it translated name 
    tmp_dir="" 
    if [ ! -d "$desktop_folder_name" ]; then
        tmp_dir=`cat /home/$SUDO_USER/.config/user-dirs.dirs | grep XDG_DESKTOP_DIR | awk -F= '{print $2}' | awk -F\" '{print $2}' | awk -F\/ '{print $2}'`
    fi
    
    # don't overwrite if tmp_dir empty or doesn't exist
    if [ -n "$tmp_dir" ];  then
        # calculate the full folder
        tmp_dir_full="/home/${SUDO_USER}/${tmp_dir}"
        if [ -d "$tmp_dir_full" ]; then
            desktop_folder_name=$tmp_dir_full
        fi
    fi

    file="$desktop_folder_name/${appli_name}.desktop"
    if [ -f "$file" ]; then
        rm $file
    fi
fi

# clean the destination folder
file="/usr/local/bin/${appli_name}"
if [ -f "$file" ]; then
    rm $file
fi


