PROJECT := memerist
BUILD   := build
REL     := build-release

.PHONY: all release run test install dist clean clean-all reconfigure fmt help

all: $(BUILD)/build.ninja
	meson compile -C $(BUILD)

$(BUILD)/build.ninja:
	meson setup $(BUILD)

release: $(REL)/build.ninja
	meson compile -C $(REL)

$(REL)/build.ninja:
	meson setup --buildtype=release $(REL)

run: all
	./$(BUILD)/src/$(PROJECT) $(ARGS)

run-release: release
	./$(REL)/src/$(PROJECT) $(ARGS)

test: all
	meson test -C $(BUILD)

install: release
	meson install -C $(REL)

dist: $(BUILD)/build.ninja
	meson dist -C $(BUILD)

clean:
	rm -rf $(BUILD)

clean-all:
	rm -rf $(BUILD) $(REL) meson-dist/

reconfigure:
	meson setup --reconfigure $(BUILD)

fmt:
	find src -type f \( -name '*.[ch]' -o -name '*.[ch]pp' \) -exec clang-format -i {} +

help:
	@echo "Targets: all (debug), release, run, run-release, test, install, dist, clean, clean-all, reconfigure, fmt"
	@echo "Usage:   make run ARGS='--my-flag'"