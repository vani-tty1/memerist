<div align="center">

<img src="data/icons/hicolor/scalable/apps/io.github.vani_tty1.memerist.svg" alt="Memerist logo" width="128"/>

# Memerist

**Memes go BRRRRRRRRRRRRR**

A modern meme editor built with GTK 4 and Libadwaita.
</div>

[![Copr build status](https://copr.fedorainfracloud.org/coprs/vaniiiiii/memerist/package/memerist/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/vaniiiiii/memerist/package/memerist/)

---
## Installation

### Flathub

<a href='https://flathub.org/apps/details/io.github.vani_tty1.memerist'>
    <img width='200' alt='Get it on Flathub' src='https://flathub.org/api/badge?locale=en'/>
</a>

## Building from Source

[GNOME Builder](https://flathub.org/en/apps/org.gnome.Builder) is the recommended way to build this app,
the IDE handles all dependencies for you via the GNOME SDK, so you can skip the Prerequisites section entirely.
If you'd rather not use Builder, follow the manual instructions below.

### Prerequisites

> **Note:** This is only required if you're building outside GNOME Builder.

Install the following development packages:

| Package               | Purpose                          |
|------------------------|-----------------------------------|
| `gcc`                  | C compiler                       |
| `make`                 | Build automation                 |
| `gtk4-devel`           | Core UI toolkit                  |
| `libadwaita-devel`     | GNOME design components          |
| `meson`                | Build system                     |
| `ninja`                | Build backend                    |
| `blueprint-compiler`   | UI markup compiler               |
| `libepoxy-devel`       | OpenGL stuff                     |
| `ImageMagick`          | GIF export support               |
| `ImageMagick-devel`    | MagickWand API for image processing libraries  |
| `pkgconf`              | Provides `pkg-config`            |
| `glib2-devel`          | Provides `glib-compile-schemas`  |
| `gettext`              | Provides `msgfmt`, `msginit`, `msgmerge`, `xgettext` for translations    |



Optional packages to be installed:

 Package                 | Purpose                                         |
|--------------------------|-----------------------------------------------|
| `appstream`              | Provides `appstreamcli` to validate the AppStream metadata     |
| `desktop-file-utils`     | Provides `desktop-file-validate` and `update-desktop-database` |
| `clang-tools-extra`      | Provides `clang-format`, used by `make fmt`                    |


### Build Instructions

```bash
# Clone the repository and enter the directory
git clone https://github.com/vani-tty1/memerist.git
cd memerist

# Compile a debug build (creates a 'build' directory)
make all

# Rebuild and run the application
# Use this after making changes to quickly compile and launch
make run

# Compile an optimized release version (creates a 'build-release' directory)
make release
make run-release

# Remove the debug build directory
make clean

# Remove all compiled build directories
make clean-all

# View more options:
make help
```


## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.

> **Note:** Please submit pull requests against the `unstable` branch.

## License

This project is open source. See the [LICENSE](LICENSE) file for details.

## Acknowledgments

Built with [GTK4](https://gtk.org/) and [Libadwaita](https://gnome.pages.gitlab.gnome.org/libadwaita/).
