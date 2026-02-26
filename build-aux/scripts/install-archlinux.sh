#!/bin/bash
set -e


rm -rf memerist_build

mkdir -p memerist_build
cd memerist_build

echo "Downloading build script..."
curl -L -O https://github.com/vani-tty1/memerist/releases/latest/download/PKGBUILD

echo "Building and Installing..."
makepkg -si --noconfirm


cd ..
rm -rf memerist_build