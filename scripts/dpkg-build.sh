#!/bin/bash
set -e
meson compile -C build


SPEC_FILE="memerist.spec"
CONTROL_FILE="deb-build/memerist-amd64/DEBIAN/control"

VERSION=$(grep "^Version:" $SPEC_FILE | awk '{print $2}')
RELEASE=$(grep "^Release:" $SPEC_FILE | awk '{print $2}' | sed 's/%{?dist}//')

cat > $CONTROL_FILE << EOF
Package: memerist
Version: ${VERSION}-${RELEASE}
Section: utils
Priority: optional
Architecture: amd64
Depends: libgtk-4-1, libglib2.0-0, libadwaita-1-0
Maintainer: Vani1-2 <giovannirafanan609@gmail.com>
Description: Meme editor for the GNOME Desktop
EOF


# NOTE: Uncomment this if you're on a Debian based system!!!!!

# cp build/src/memerist deb-build/memerist-amd64/usr/bin/
# dpkg-deb --build deb-build/memerist-amd64




# NOTE: All of the code below should be commented or removed if you are on debian
# system as you only need this if you have distrobox with an ubuntu image to build
# a native .deb package
distrobox enter ubuntu -- bash -c '
  cp build/src/memerist deb-build/memerist-amd64/usr/bin/
  dpkg-deb --build deb-build/memerist-amd64
'

distrobox-stop ubuntu -Y