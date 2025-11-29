Name:           Memerist
Version:        0.0.40.beta.2
Release:        1%{?dist}
Summary:        Meme generator with text overlays
License:        GPL-3.0-or-later
URL:            https://github.com/Vani1-2/gnome-meme-generator
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  meson gcc pkgconfig(gtk4) pkgconfig(libadwaita-1) pkgconfig(cairo)
Requires:       gtk4 libadwaita

%description
Create memes with custom text overlays.

%prep
%autosetup -p1

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
%{_datadir}/Memerist/templates/*

%post
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
/usr/bin/update-desktop-database &> /dev/null || :
mkdir -p /usr/local/share/icons/hicolor/scalable/apps
mkdir -p /usr/local/share/icons/hicolor/symbolic/apps
ln -sf /usr/share/icons/hicolor/scalable/apps/org.gnome.Memerist.svg /usr/local/share/icons/hicolor/scalable/apps/org.gnome.Memerist.svg
ln -sf /usr/share/icons/hicolor/symbolic/apps/org.gnome.Memerist-symbolic.svg /usr/local/share/icons/hicolor/symbolic/apps/org.gnome.Memerist-symbolic.svg
/usr/bin/gtk-update-icon-cache /usr/local/share/icons/hicolor &> /dev/null || :
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :

%postun
/usr/bin/glib-compile-schemas %{_datadir}/glib-2.0/schemas &> /dev/null || :
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &> /dev/null || :
/usr/bin/update-desktop-database &> /dev/null || :
rm -f /usr/local/share/icons/hicolor/scalable/apps/org.gnome.Memerist.svg
rm -f /usr/local/share/icons/hicolor/symbolic/apps/org.gnome.Memerist-symbolic.svg


%changelog
* Fri Nov 28 2025 Giovanni <giovanni@example.com> - 0.0.38.beta.5
- Fixed drag and drop geometry for text
- Initialized drag variables to prevent compiler warnings