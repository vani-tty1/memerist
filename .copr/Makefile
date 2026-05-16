srpm:
	dnf install -y git rpmautospec
	$(eval VERSION := $(shell grep "Version:" *.spec | awk '{print $$2}'))
	git clone --depth=1 --branch testing https://github.com/vani-tty1/memerist.git memerist-$(VERSION)
	tar -czf memerist-$(VERSION).tar.gz memerist-$(VERSION)/
	rm -rf memerist-$(VERSION)/
	rpmautospec process-distgit . processed.spec
	rpmbuild -bs --define "_sourcedir ." --define "_srcrpmdir $(outdir)" processed.spec
