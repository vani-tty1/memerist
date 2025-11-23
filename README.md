  ![org gnome Memerist](https://github.com/user-attachments/assets/12d959d2-25e9-4da1-9d6b-9f5ea1381d15)


# GNOME Meme Generator

The ultimate tool for creating memes built upon GTK-4 and Libadwaita for the GNOME desktop.

## Features
- Import images
- Add top and bottom text
- Export memes as PNG
- Clean, native GNOME interface
- Let It Happen
# Sample

<img width="950" height="750" alt="Screenshot From 2025-11-15 15-29-31" src="https://github.com/user-attachments/assets/691d54c8-c16e-4741-995a-4244e0cf34c2" />
<img width="950" height="750" alt="image" src="https://github.com/user-attachments/assets/97e6a40d-1434-4f1b-b672-703a0a6941bb" />
<img width="950" height="750" alt="image" src="https://github.com/user-attachments/assets/350843ca-83be-4de2-88e4-73e324774602" />



# Building
## Prerequisite's
You need to install this packages in order to build this program:
- gtk4-devel 
- libadwaita-devel 






## Building

```bash
git clone https://github.com/Vani1-2/gnome-meme-generator.git
cd gnome-meme-generat(Memerist:109581): Gtk-WARNING **: 18:14:54.357: Failed to load icon /usr/local/share/icons/hicolor/scalable/apps/org.gnome.Memerist.svg: Error opening file /usr/local/share/icons/hicolor/scalable/apps/org.gnome.Memerist.svg: No such file or directory
or
meson setup build
meson compile -C build
#Optional
sudo meson install -C build
