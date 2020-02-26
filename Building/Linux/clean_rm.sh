#! /bin/bash
set -e

#we need to rm files that have been copied via script
rep="/usr/share/icons/OSCAR"
if [ -d "$rep" ]; then
    rm -r $rep
fi

rep="/usr/share/doc/OSCAR"
if [ -d "$rep" ]; then
    rm -r $rep
fi

rep="/usr/share/OSCAR"
if [ -d "$rep" ]; then
    rm -r $rep
fi

