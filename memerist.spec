Name:           memerist
Version:        0.0.79.beta.1
Release:        2%{?dist}
Summary:        Meme generator with text overlays
License:        GPL-3.0-or-later
URL:            https://github.com/Vani1-2/gnome-meme-editor
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
%dir %{_datadir}/io.github.vani1_2.memerist
%{_datadir}/io.github.vani1_2.memerist/templates/


%changelog
* Sun Dec 07 2025 Giovanni <giovannirafanan609@gmail.com> - 0.0.79.beta.1-2
- finally fixed dependency hell
