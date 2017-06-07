#!/bin/bash
# first build package using python setp tool
python setup.py --command-packages=stdeb.command bdist_deb

mkdir -p ./deb_dist/pijuice-1.0/bin/
cp -a ./bin/. ./deb_dist/pijuice-1.0/bin/

cp -a ./debian/. ./deb_dist/pijuice-1.0/debian/

#debuild -us -uc
(cd ./deb_dist/pijuice-1.0 && dpkg-buildpackage -b -rfakeroot -us -uc)