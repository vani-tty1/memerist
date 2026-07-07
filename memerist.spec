Name:           memerist
Version:        0.9.0~upstream
Release:        %autorelease
Summary:        Meme generator with text overlays
License:        GPL-3.0-or-later
URL:            https://github.com/vani-tty1/memerist
Source0:        memerist-%{version}.tar.gz
BuildRequires:  meson gcc pkgconfig(gtk4) pkgconfig(libadwaita-1) pkgconfig(cairo)
BuildRequires:  desktop-file-utils blueprint-compiler libepoxy-devel ImageMagick ImageMagick-devel
Requires:       gtk4 libadwaita ImageMagick
%description
Create memes with custom text overlays using a native GNOME interface.

%prep
%autosetup -n memerist-%{version}

%build
%meson 
%meson_build

%install
%meson_install

%files
%{_bindir}/memerist
%{_datadir}/applications/io.github.vani_tty1.memerist.desktop
%{_datadir}/glib-2.0/schemas/io.github.vani_tty1.memerist.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.vani_tty1.memerist.svg
%{_datadir}/icons/hicolor/symbolic/apps/io.github.vani_tty1.memerist-symbolic.svg
%{_datadir}/metainfo/io.github.vani_tty1.memerist.metainfo.xml


%changelog
* Thu Jul 7 2026 Giovanni Rafanan <giovannirafanan609@gmail.com> - 0.9.0-1
- Migrated image filters to MagickWand
- Fixed tearing and generation loss when adding too many layers on a high res base image
- Total rewrite of renderer
- Improve dragging smoothness of image layers on a high res base image
- Make recently imported templates appear at the top of the list
- Fixed use-after-free which causes the app to crash when saving a project
