#!/bin/bash
file="./file"
if [ -e $file ]; then
	echo "File exists"
else 
	echo "File does not exists"
fi

while [ ! -e myfile ]; do
# Sleep until file does exists/is created
sleep 1
done

