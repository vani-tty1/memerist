PROJECT := memerist
BUILD   := build
REL     := build-release

BINS := meson ninja msgfmt appstreamcli desktop-file-validate glib-compile-schemas blueprint-compiler pkg-config msginit msgmerge xgettext gtk4-update-icon-cache update-desktop-database
LIBS := gtk4 libadwaita-1 cairo epoxy gio-2.0

.PHONY: all release run test install dist clean clean-all reconfigure fmt check-deps help

all: check-deps $(BUILD)/build.ninja
	meson compile -C $(BUILD)

release: check-deps $(REL)/build.ninja
	meson compile -C $(REL)

check-deps:
	@failed=0; \
	echo "Checking program dependencies..."; \
	for bin in $(BINS); do \
		if command -v $$bin >/dev/null 2>&1; then \
			printf "  %-25s \033[32m[OK]\033[0m\n" "$$bin"; \
		else \
			printf "  %-25s \033[31m[FAILED]\033[0m\n" "$$bin"; \
			failed=1; \
		fi; \
	done; \
	echo "Checking library dependencies..."; \
	for lib in $(LIBS); do \
		if pkg-config --exists $$lib >/dev/null 2>&1; then \
			printf "  %-25s \033[32m[OK]\033[0m\n" "$$lib"; \
		else \
			printf "  %-25s \033[31m[FAILED]\033[0m\n" "$$lib"; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -ne 0 ]; then \
		echo "\n\033[31mError: Missing dependencies. Please install the failed items.\033[0m"; \
		exit 1; \
	fi

$(BUILD)/build.ninja:
	meson setup $(BUILD)

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
	@command -v clang-format >/dev/null 2>&1 || { echo "Error: clang-format not found"; exit 1; }
	find src -type f \( -name '*.[ch]' -o -name '*.[ch]pp' \) -exec clang-format -i {} +

help:
	@echo "Targets: all (debug), release, run, run-release, test, install, dist, clean, clean-all, reconfigure, fmt, check-deps"
	@echo "Usage:   make run ARGS='--my-flag'"