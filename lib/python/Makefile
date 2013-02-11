.PHONY: all libs clean install

all: libs

libs:
	python setup.py build

clean:
	python setup.py clean
	$(RM) mcontrol/mcontrol.c

install:
	python setup.py install --prefix $(DESTDIR)/usr --install-scripts $(BIN_DIR)/
