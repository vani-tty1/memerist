### Overview 

> **Note:** [GNOME Builder](https://flathub.org/en/apps/org.gnome.Builder) is the recommended way to build this app,
the IDE handles all dependencies for you via the GNOME SDK, so you can skip the [Prerequisites](#prerequisites) section entirely.

Memerist uses the [`meson`](https://mesonbuild.com/) build system to configure the build and detect dependencies
and [`ninja`](https://ninja-build.org/) as its backend compile it, this makes life easier.
You generally won't need to touch either directly if you use BUILDER or the [make targets](#build-instructions).


You can choose between two build manifest for GNOME Builder to use found in the [build-aux](build-aux/) directory:

1. The normal one which does not have the devel build options set, this is same setup used 
for the official [flatpak build](https://github.com/flathub/io.github.vani_tty1.memerist).

2. The one that ends with `.Devel` which the name suggest for development purposes, this sets the build profile to `development`.
This will result in a bigger build dir and consumes more memory as it contains more debug symbols, 
so use this when you want to experiment with the source code.
 
> **NOTE:** If you'd rather not use Builder, follow the manual instructions below.




### Prerequisites

Install the following development packages:
> NOTE: on some distros, the development headers are separate with the base package so you may need to install the devel packages(e.g. `gtk4-devel`)  

| Package              	 | Purpose                          |
|------------------------|----------------------------------|
| `gcc`                  | C compiler                       |
| `make`                 | Build automation                 |
| `gtk4`          		 | Core UI toolkit                  |
| `libadwaita`    		 | GNOME design components          |
| `meson`                | Build system                     |
| `ninja`                | Build backend                    |
| `blueprint-compiler`   | UI markup compiler               |
| `libepoxy`       		 | OpenGL stuff                     |
| `ImageMagick`          | GIF export support and MagickWand API for image processing libraries  |
| `pkgconf`              | Provides `pkg-config`            |
| `glib2`         		 | Provides `glib-compile-schemas`  |
| `gettext`              | Provides `msgfmt`, `msginit`, `msgmerge`, `xgettext` for translations    |



Optional packages to be installed:

| Package                 | Purpose                                         |
|--------------------------|-----------------------------------------------|
| `appstream`              | Provides `appstreamcli` to validate the AppStream metadata     |
| `desktop-file-utils`     | Provides `desktop-file-validate` and `update-desktop-database` |


### Build Instructions

```bash
# Clone the repository and enter the directory
git clone https://github.com/vani-tty1/memerist.git
cd memerist

# Compile and run:
make run

# View more options:
make help
```


### Packaging

If you're interested in packaging Memerist for a distro, contributions are welcome! 
An existing `.spec` file (for RPM-based distros) and a `PKGBUILD` (for Arch) can be
used as a starting point or reference for other formats:

1. The `.spec` file can be found in the copr branch of this repo
```bash 
git switch copr
```

2. Two `PKGBUILD`'s exist within the AUR, one for a vcs package and one for the tagged release,
I generally recommend the `-git` PKGBUILD as it uses `arch-meson` which applies Arch's recommended Meson build flags.
- [memerist-git](https://aur.archlinux.org/packages/memerist-git) 
- [memerist](https://aur.archlinux.org/packages/memerist)


A few things to keep in mind:

- The app ID is `io.github.vani_tty1.memerist`, please keep this consistent so config paths 
  and desktop integration all resolve correctly.
- Memerist builds with `meson`/`ninja`; see the [Prerequisites](#prerequisites) and manual build
  instructions above for the full dependency list.
- Releases are tagged on GitHub with source tarballs, please build from a
  tagged release rather than an arbitrary commit.
- If you maintain a package, feel free to open a PR adding it to this section (with a badge/link), or open
  an issue so it can be linked here.

