# GNOME Meme Editor

<p align="center">
  <img src="data/icons/hicolor/scalable/apps/io.github.vani1_2.memerist.svg" alt="GNOME Meme Editor Logo" width="128"/>
</p>

<p align="center">
  <strong>Memes go BRRRRRRRRRRRRR</strong>
</p>

<p align="center">
  A modern meme editor built with GTK 4 and Libadwaita for the GNOME desktop.
</p>

<p align="left">
  <a href="https://copr.fedorainfracloud.org/coprs/vaniiiiii/memerist">
    <img src="https://img.shields.io/badge/📦_Copr-294172?style=for-the-badge&logo=fedora&logoColor=white" alt="Copr">
  </a> 
</p>

[![Copr build status](https://copr.fedorainfracloud.org/coprs/vaniiiiii/memerist/package/memerist/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/vaniiiiii/memerist/package/memerist/)


---

## Features
- **Use or Import your own Templates** 
- **Image Import** - Load any image to use as your meme template
- **Classic Meme Text** - You can drag the text anywhere in the photo
- **PNG Export** 
- **Native GNOME Design** - Built with Libadwaita
- **Let it Happen**

## Screenshots

<p align="center">
 <img width="800" alt="meme example 1" src="assets/screenshot1.png" />
</p>

<p align="center">
 <img width="800" alt="meme example 2" src="assets/screenshot2.png"/>
</p>


## Installation

### Fedora Copr
Install latest build using Copr( Supports amd64 & aarch64 ):
    
```bash
sudo dnf copr enable vaniiiiii/memerist 
sudo dnf install memerist
```
### Debian/Ubuntu
Download the latest `.deb` file using this command( only available as amd64 )
```bash
curl -L -O https://raw.githubusercontent.com/Vani1-2/gnome-meme-editor/main/deb-build/memerist-amd64.deb
```




### Building from Source

#### Prerequisites

Install the required development packages:

**Fedora/RHEL:**
```bash
sudo dnf install gtk4-devel libadwaita-devel meson ninja-build
```

**Ubuntu/Debian:**
```bash
sudo apt install libgtk-4-dev libadwaita-1-dev meson ninja-build
```

**Arch Linux:**
```bash
sudo pacman -S gtk4 libadwaita meson ninja
```

#### Build Instructions

```bash
# Clone the repository
git clone https://github.com/Vani1-2/gnome-meme-editor.git
cd gnome-meme-editor

# Configure the build
meson setup build

# Compile
meson compile -C build

# Install (optional)
sudo meson install -C build
```

#### Running Without Installing

```bash
./build/src/memerist
```

##  Usage

1. Launch Memerist from your application menu
2. Click the folder button to browse images using your file browser
3. Enter your top and bottom text, you can drag the text anywhere
4. Export your meme as PNG
5. Let it Happen

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.

## License

This project is open source. Please check the COPYING file for details.

## Acknowledgments

Built with GTK 4 and Libadwaita for the GNOME desktop environment.

---

<p align="center">
  Made with a lot of Redbull
</p>

