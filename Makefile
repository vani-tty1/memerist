.PHONY: build run clean reconfigure release run-release

build: build/build.ninja
	meson compile -C build

build/build.ninja:
	meson setup build

run: build
	./build/src/memerist
	
run-release: release
	./build-release/src/memerist
	
clean:
	rm -rf build build-release

release: build-release/build.ninja
	meson compile -C build-release

build-release/build.ninja:
	meson setup --buildtype=release build-release

reconfigure:
	meson setup --reconfigure build