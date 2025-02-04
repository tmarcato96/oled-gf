#!/bin/bash

for configfile in $2/*.json
do
     $1 $configfile
done