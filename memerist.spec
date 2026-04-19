Name:           memerist
Version:        0.7.0~preview
Release:        3%{?dist}
Summary:        Meme generator with text overlays
License:        GPL-3.0-or-later
URL:            https://github.com/vani-tty1/memerist
Source0:        memerist-%{version}.tar.gz
BuildRequires:  meson gcc pkgconfig(gtk4) pkgconfig(libadwaita-1) pkgconfig(cairo) desktop-file-utils blueprint-compiler libepoxy-devel
Requires:       gtk4 libadwaita
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
* Sat Apr 11 2026 vani-tty1 <giovannirafanan609@gmail.com> - 0.6.2-1
- build from the latest commits of testing branch