#!/bin/bash
#First you can use grep (-n) to find the number of lines of string.
#Then you can use awk to separate the answer.
# $1 file  $2 string $3 result
grep -in $2 $1 | awk -F: '{print $1}' > $3 
