#! /bin/bash
set -e
#
#test USER (must be root) vs SUDO_USER (must be other than root but not empty)
if [ "$USER" != "root" ] || [ "$SUDO_USER" = "root" ] || [ -z "$SUDO_USER" ]; then
    echo "Installation must be done as normal user with sudo command. fatal error"
    exit
fi
#    do nothing

