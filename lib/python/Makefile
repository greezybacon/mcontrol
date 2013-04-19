include ../../Makefile.in

.PHONY: all libs clean install

all: libs

libs:
	GIT_VERSION="${GIT_VERSION}" python setup.py build

clean:
	python setup.py clean
	$(RM) mcontrol/mcontrol.c

install:
	GIT_VERSION="${GIT_VERSION}" python setup.py install \
		--prefix $(DESTDIR)/usr --install-scripts $(BIN_DIR)/
