#!/bin/sh

TOP=$(dirname $0)
APP="$PWD/test.cpp"
ext=`echo $APP | tr '.' '\n' | tail -1`
filename=`basename $APP .$ext`
what_file=$(/bin/sh -x ${TOP}/create-what-source.sh ${PWD}/${filename})
GPP=`which g++`
if [ "x$GPP" = "x" ]; then
CXX=clang++
else
CXX=g++
fi
cmd="$CXX -g2 -O2 -o $filename $what_file $APP"
echo "CMD: $cmd"
$cmd
