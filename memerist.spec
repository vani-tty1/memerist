Name:           memerist
Version:        0.3.1
Release:        1%{?dist}
Summary:        Meme generator with text overlays
License:        GPL-3.0-or-later
URL:            https://github.com/Vani1-2/memerist
Source0:        memerist-%{version}.tar.gz
BuildRequires:  meson gcc pkgconfig(gtk4) pkgconfig(libadwaita-1) pkgconfig(cairo) desktop-file-utils
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
%{_datadir}/applications/io.github.vani1_2.memerist.desktop
%{_datadir}/glib-2.0/schemas/io.github.vani1_2.memerist.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/io.github.vani1_2.memerist.svg
%{_datadir}/icons/hicolor/symbolic/apps/io.github.vani1_2.memerist-symbolic.svg
%{_datadir}/metainfo/io.github.vani1_2.memerist.metainfo.xml


%changelog
* Sat Feb 21 2026 vani-tty1 <giovannirafanan609@gmail.com> - 0.3.1-1
- code refactoring and decluttering
- contrast filter now works on templates
- fixed keyboard menu not showing in the header menu