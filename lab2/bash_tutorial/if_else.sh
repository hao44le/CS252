#!/bin/bash
directory="./Scripting"
#if states to check if directory exists
if [ -d $directory ];then
  echo "Directory exists"
else
  echo "Directory does not exist"
fi
