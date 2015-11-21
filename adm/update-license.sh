#!/bin/sh

if [ -e adm ]; then
	adm=adm
else
	adm=../adm
fi

update_license() {
	file=$1
	{
	    cp $file $file.BAK &&
	    awk -f $adm/update-license.awk $file > $file.$$ &&
	    cp $file.$$ $file &&
	    rm $file.$$
	} ||
	    echo WARNING: could not update license for $file 1>&2
}

for f
do
	echo Updating $f
	update_license $f
done
