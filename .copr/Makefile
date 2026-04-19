srpm:
	dnf install -y git
	$(eval VERSION := $(shell grep "Version:" *.spec | awk '{print $$2}'))
	git clone --depth=1 --branch main https://github.com/vani-tty1/memerist.git memerist-$(VERSION)
	tar -czf memerist-$(VERSION).tar.gz memerist-$(VERSION)/
	rm -rf memerist-$(VERSION)/
	rpmbuild -bs --define "_sourcedir ." --define "_srcrpmdir $(outdir)" *.spec