#!/bin/bash


URL="https://raw.githubusercontent.com/Vani1-2/gnome-meme-editor/main/deb-build/memerist-amd64.deb"
FILE="memerist-amd64.deb"


if ! command -v apt &> /dev/null; then
    echo "Error: This script requires 'apt' (Debian/Ubuntu)."
    exit 1
fi


echo "Downloading memerist..."
if command -v curl &> /dev/null; then
    curl -L -o "$FILE" "$URL"
elif command -v wget &> /dev/null; then
    wget -O "$FILE" "$URL"
else
    echo "Error: 'curl' or 'wget' not found, install them."
    exit 1
fi


if [ -f "$FILE" ]; then
    echo "Installing..."
    sudo apt install "./$FILE" -y

    echo "Cleaning up..."
    rm "$FILE"
    echo "Installation complete!"
else
    echo "Error: Download failed."
    exit 1
fi