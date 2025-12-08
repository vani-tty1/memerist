Name:           memerist
Version:        0.0.76.beta.5
Release:        1%{?dist}
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
%{_datadir}/applications/org.gnome.Memerist.desktop
%{_datadir}/glib-2.0/schemas/org.gnome.Memerist.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/org.gnome.Memerist.svg
%{_datadir}/icons/hicolor/symbolic/apps/org.gnome.Memerist-symbolic.svg
%dir %{_datadir}/Memerist
%{_datadir}/Memerist/templates/



%changelog
* Sun Dec 07 2025 Giovanni <giovannirafanan609@gmail.com> - 0.0.76.beta.5
- minor bug fix