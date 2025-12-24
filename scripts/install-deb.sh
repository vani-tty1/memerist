#!/bin/bash


REPO="Vani1-2/gnome-meme-editor"

if ! command -v apt &> /dev/null; then
    echo "Error: This script requires 'apt' (Debian/Ubuntu)."
    exit 1
fi

echo "Finding latest release..."


DOWNLOAD_URL=$(curl -s "https://api.github.com/repos/$REPO/releases/latest" | grep -o "https://.*\.deb" | head -n 1)

if [ -z "$DOWNLOAD_URL" ]; then
    echo "Error: Could not find a .deb file in the latest release."
    exit 1
fi

FILE=$(basename "$DOWNLOAD_URL")

echo "Downloading $FILE..."


if command -v curl &> /dev/null; then
    curl -L -o "$FILE" "$DOWNLOAD_URL"
elif command -v wget &> /dev/null; then
    wget -O "$FILE" "$DOWNLOAD_URL"
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