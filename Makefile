.PHONY: build clean setup run reconfigure

reconfigure:
	meson setup --reconfigure build

setup:
	meson setup build

build:
	meson compile -C build

run:
	./build/src/memerist

clean:
	rm -rf build