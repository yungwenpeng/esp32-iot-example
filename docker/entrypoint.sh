#!/bin/bash

USER_ID=${DEV_UID:-9001}
GROUP_ID=${DEV_GID:-9001}
groupadd -g $GROUP_ID build
useradd --shell /bin/bash -u $USER_ID -g $GROUP_ID -o -m build

export HOME=/home/user
export USER=build
export TERM=xterm
export LANG=en_US.UTF-8
export LANGUAGE=en_US:en

$IDF_PATH/tools/idf_tools.py --non-interactive install required
$IDF_PATH/tools/idf_tools.py --non-interactive install cmake
$IDF_PATH/tools/idf_tools.py --non-interactive install-python-env
. $IDF_PATH/export.sh

exec /usr/local/bin/gosu build "$@"
