#!/bin/sh
#
# A script to fix the DirectX includes so that gcc can compile them...

#cd `dirname $0`
for file in include/directx6/*.h
do echo "Stripping $file..."
   tr -d '\r' <$file >$file.new && mv $file.new $file
done

# Fix the few remaining headers that have anonymous unions
for file in include/directx6/d3dtypes.h
do if [ -f $file ]; then
     echo "Fixing $file..."
     perl ./deunion.pl <$file >$file.new && mv $file.new $file
   fi
done
