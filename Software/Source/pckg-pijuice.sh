#!/bin/bash
# first build package using python setp tool
if [[ "$@" == "--light" ]]
then
    echo "Building light version..."
    export PIJUICE_BUILD_LIGHT=1
fi

python setup.py --command-packages=stdeb.command bdist_deb

mkdir -p ./deb_dist/pijuice-1.1/bin/
cp -a ./bin/. ./deb_dist/pijuice-1.1/bin/

cp -a ./debian/. ./deb_dist/pijuice-1.1/debian/

#debuild -us -uc
(cd ./deb_dist/pijuice-1.1 && dpkg-buildpackage -b -rfakeroot -us -uc)