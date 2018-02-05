#!/bin/bash
export PIJUICE_VERSION=$(python -c "import pijuice; print pijuice.__version__")

# Build base package
export PIJUICE_BUILD_BASE=1

python setup.py --command-packages=stdeb.command bdist_deb

mkdir -p ./deb_dist/pijuice-base-$PIJUICE_VERSION/bin/
cp -a ./bin/. ./deb_dist/pijuice-base-$PIJUICE_VERSION/bin/

cp -a ./debian-base/. ./deb_dist/pijuice-base-$PIJUICE_VERSION/debian/
(cd ./deb_dist/pijuice-base-$PIJUICE_VERSION && dpkg-buildpackage -b -rfakeroot -us -uc)

mv deb_dist deb_dist_base

# Build GUI package
unset PIJUICE_BUILD_BASE

python setup.py --command-packages=stdeb.command bdist_deb

cp -a ./debian-gui/. ./deb_dist/pijuice-gui-$PIJUICE_VERSION/debian/
(cd ./deb_dist/pijuice-gui-$PIJUICE_VERSION && dpkg-buildpackage -b -rfakeroot -us -uc)
