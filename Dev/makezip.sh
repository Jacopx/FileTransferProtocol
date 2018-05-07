#!/bin/bash
zipname=lab_2.3_3.1
cp ${zipname}.zip .${zipname}.zip.old >& /dev/null
zip -r ${zipname}.zip.new source/client[12]/*.[ch] source/server[12]/*.[ch] source/*.[chx]
if [[ $? == 0 ]] ; then
    if [[ -e "${zipname}.zip" ]] ; then
        rm ${zipname}.zip
    fi
    mv ${zipname}.zip.new ${zipname}.zip
fi
