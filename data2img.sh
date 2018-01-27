#!/bin/sh
# Wrapper around data2img with compression

DATA2IMG="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"/data2img
COMPRESSION="gzip" # Compression command
EXTENSION="gz" # Compression file extension

# Validate input
if [ -z "$3" ]; then
	$DATA2IMG
	exit $?
fi
if ! [ -f "$2" ]; then
	echo "Input file not found.";
	exit 128;
fi
if [ -f "$3" ]; then
	echo "Output file already exists!";
	exit 128;
fi

# Execute data2img with $COMPRESSION
set -e
if [ "$1" == "encode" ]; then
	$COMPRESSION -c "$2" > "/tmp/data2img_tmp";
	if [ -z "$4" ]; then
		$DATA2IMG encode "/tmp/data2img_tmp" "$3";
	else
		$DATA2IMG encode "/tmp/data2img_tmp" "$3" "$4";
	fi
	rm /tmp/data2img_tmp;
elif [ "$1" == "decode" ]; then
	$DATA2IMG decode "$2" "/tmp/data2img_tmp.$EXTENSION";
	$COMPRESSION -d "/tmp/data2img_tmp.$EXTENSION";
	mv "/tmp/data2img_tmp" "$3";
else
	echo "Invalid Command."
fi