Name:           memerist
Version:        1.0.0
Release:        2%{?dist}
Summary:        Meme generator with text overlays
License:        GPL-3.0-or-later
URL:            https://github.com/vani-tty1/memerist
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz
BuildRequires:  meson gcc pkgconfig(gtk4) pkgconfig(libadwaita-1) pkgconfig(cairo) appstream
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

%check
%meson_check


%files
%{_bindir}/memerist
%{_datadir}/applications/io.github.vani_tty1.memerist.desktop
%{_datadir}/glib-2.0/schemas/io.github.vani_tty1.memerist.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.vani_tty1.memerist.svg
%{_datadir}/icons/hicolor/symbolic/apps/io.github.vani_tty1.memerist-symbolic.svg
%{_datadir}/metainfo/io.github.vani_tty1.memerist.metainfo.xml


%changelog
* Thu Jul 23 2026 Giovanni Rafanan <giovannirafanan609@gmail.com> - 1.0.0-2
- UI Overhaul. The UI is greatly overhauled to make it more organized.
- UI now adapts better to different screen sizes/devices.
- Fixed a bug where you cannot exit template selection on narrow window mode.
- Support for opening files from the File Manager(e.g., Nautilus, Thunar, Dolphin) using the "Open With" option
