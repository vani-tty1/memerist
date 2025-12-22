#!/bin/bash
set -e


rm -rf memerist_build

mkdir -p memerist_build
cd memerist_build

echo "Downloading build script..."
curl -L -O https://raw.githubusercontent.com/Vani1-2/gnome-meme-editor/main/PKGBUILD

echo "Building and Installing..."
makepkg -si --noconfirm


cd ..
rm -rf memerist_build