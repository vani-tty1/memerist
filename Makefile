PROJECT := memerist
BUILD   := build
REL     := build-release

BINS := meson ninja msgfmt appstreamcli \
		desktop-file-validate glib-compile-schemas\
		blueprint-compiler pkg-config msginit msgmerge\
		xgettext gtk4-update-icon-cache update-desktop-database magick
LIBS := gtk4 libadwaita-1 cairo epoxy gio-2.0

.PHONY: all release run test install dist clean reconfigure check-deps help

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
			printf "  %-25s \033[31m[ERROR]\033[0m\n" "$$bin"; \
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
		printf "\n\033[31mError: Missing dependencies. Please install required items.\033[0m\n"; \
		exit 1; \
	fi

$(BUILD)/build.ninja:
	meson setup $(BUILD) -Dprofile=development

$(REL)/build.ninja:
	meson setup --buildtype=release $(REL)

run: all
	dconf reset -f /io/github/vani_tty1/memerist/
	meson devenv -C $(BUILD) ./src/$(PROJECT) $(ARGS)

run-release: release
	dconf reset -f /io/github/vani_tty1/memerist/
	meson devenv -C $(REL) ./src/$(PROJECT) $(ARGS)

test: all
	meson test -v -C $(BUILD)

install: release
	meson install -C $(REL)

dist: $(REL)/build.ninja
	meson dist -C $(REL) --allow-dirty

clean:
	rm -rf $(BUILD) $(REL) meson-dist/

reconfigure:
	meson setup --reconfigure $(BUILD)

help:
	@echo "Targets: all (debug), release, run, run-release, test, install, dist, clean, reconfigure, check-deps"
	@echo "Usage:   make run ARGS='--my-flag'"
