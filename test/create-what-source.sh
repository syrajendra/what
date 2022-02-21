#!/bin/sh

if [ $# = 1 ]; then
	PKG_BIN="$1"
	PKG=`basename $PKG_BIN | tr '.' '\n' | head -1`
  	CPKG=`echo $PKG | tr [a-z] [A-Z]`
	PKG_PATH=`dirname $PKG_BIN`
	if [ ! -d $PKG_PATH ]; then
		echo "Package binary path does not exist '$PKG_PATH'"
		exit 1
	fi
	DATE=$(date '+%Y%m%d')
  FDATE=$(date '+%F %T %Z')
	src_name=${PKG}_what.cpp
	HOST=$(hostname -f)
 	RELEASE="1.0-$DATE"
  line="volatile const char __attribute__ ((section(\".data\"))) what_line1[] = \"@(#) $CPKG release $RELEASE built by $USER on $FDATE\\\n@(#) $HOST:$PKG_PATH\";"
  echo $line  > $PKG_PATH/$src_name
	echo "" 	>> $PKG_PATH/$src_name
	echo "$PKG_PATH/$src_name"
else
	echo "Supply absolute package binary path as argument (along with package name): Ex: <path>/test"
	exit 1
fi
