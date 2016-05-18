#!/bin/bash
#hello_world with variables
STRING="HELLO WORLD!!!"
echo $STRING

#backup
OF=myhome_directory_$(date +%Y-%m-%d).tar.gz
tar -czf $OF $HOME
