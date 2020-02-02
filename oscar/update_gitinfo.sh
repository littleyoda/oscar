#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

git rev-parse --git-dir &>/dev/null
if [ $? -eq 0 ]; then
    GIT_BRANCH=`git rev-parse --abbrev-ref HEAD`
    [ "$GIT_BRANCH" == "HEAD" ] && GIT_BRANCH=""  # not really a branch
    GIT_REVISION=`git rev-parse --short HEAD`
    $(git diff-index --quiet HEAD --)
    if [ $? -ne 0 ]; then
        GIT_REVISION="${GIT_REVISION}-plus"  # uncommitted changes
    else
        # only use the tag if clean
        GIT_TAG=`git describe --exact-match --tags 2>/dev/null`
    fi
fi

echo // This is an auto generated file > $DIR/git_info.h.new
[ -n "$GIT_BRANCH" ] && echo "#define GIT_BRANCH \"$GIT_BRANCH\"" >> $DIR/git_info.h.new
[ -n "$GIT_REVISION" ] && echo "#define GIT_REVISION \"$GIT_REVISION\"" >> $DIR/git_info.h.new
[ -n "$GIT_TAG" ] && echo "#define GIT_TAG \"$GIT_TAG\"" >> $DIR/git_info.h.new

if diff $DIR/git_info.h $DIR/git_info.h.new &> /dev/null; then
    rm $DIR/git_info.h.new
else
    echo Updating $DIR/git_info.h
    mv $DIR/git_info.h.new $DIR/git_info.h
fi
