.PHONY: build clean run reconfigure

reconfigure:
	meson setup --reconfigure build

build:
	meson setup build && meson compile -C build

run:
	meson compile -C build && ./build/src/memerist

clean:
	rm -rf build