#!/bin/bash

SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT")
cd $BASEDIR

sh build.sh

./bin/CodeCraft-2022
