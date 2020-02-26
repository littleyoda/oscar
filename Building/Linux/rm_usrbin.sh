#! /bin/bash
#
# no error is permitted
set -e

# application name
appli_name="OSCAR"

if [ X_$SUDO_USER != "X_" ]; then
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

# clean destination folders (bin + shortcut) + move from /opt/OSCAR (if exists)

if [ -d "/opt/OSCAR" ]; then
    # --- old folder exists : move files to the new folder ---
    # Help folder
    folder_from="/opt/OSCAR/Help"
    folder_to="/usr/share/${appli_name}"
    if [ -d "$folder_from" ]; then
        mkdir -p $folder_to
        mv $folder_from $folder_to
    fi
    # Html folder
    folder_from="/opt/OSCAR/Html"
    folder_to="/usr/share/${appli_name}"
    if [ -d "$folder_from" ]; then
        mkdir -p $folder_to
        mv $folder_from $folder_to
    fi
    # Translations folder
    folder_from="/opt/OSCAR/Translations"
    folder_to="/usr/share/${appli_name}"
    if [ -d "$folder_from" ]; then
        mkdir -p $folder_to
        mv $folder_from $folder_to
    fi
    # icon file : OSCAR.png
    file="/opt/OSCAR/OSCAR.png"
    folder_to="/usr/share/icons/${appli_name}"
    if [ -f "$file" ]; then
        mkdir -p $folder_to
        mv $file ${folder_to}/${appli_name}.png
    fi
    # shortcut file : OSCAR.desktop
    file="/opt/OSCAR/OSCAR.desktop"
    folder_to="/usr/share/applications/${appli_name}"
    if [ -f "$file" ]; then
        mkdir -p $folder_to
        mv $file ${folder_to}/${appli_name}.desktop
    fi
    
    # what is the purpose of saving the binary file ? 
    # binary file : OSCAR (in /tmp + minimize the name [oscar instead of OSCAR])
    #file="/opt/OSCAR/OSCAR"
    #folder_to="/tmp/${appli_name}"
    #if [ -f "$file" ]; then
    #    mkdir -p $folder_to
    #    mv $file ${folder_to}/${appli_name}
    #fi
    
    # folder /opt can be deleted
    rm -R /opt/OSCAR
fi

# clean the destination folder
file="/usr/bin/${appli_name}"
if [ -f "$file" ]; then
    rm $file
fi

if [ -x /usr/bin/update-menus ]; then update-menus; fi


