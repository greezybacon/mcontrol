PREPARSE=python $(PWD)/mcodepp.py

MOTORS=mAxis.mxt nAxis.mxt oAxis.mxt pAxis.mxt uAxis.mxt vAxis.mxt \
	xAxis.mxt yAxis.mxt zAxis.mxt
BUILD=$(MOTORS:%=../../$(BUILDDIR)/dosis-1.x/%)

.PHONY: all
all: build-dir $(BUILD)

.PHONY: build-dir
build-dir:
	mkdir -p ../../$(BUILDDIR)/dosis-1.x

mAxis.mxt nAxis.mxt oAxis.mxt pAxis.mxt: mdrive_base.mxt

../../$(BUILDDIR)/dosis-1.x/%.mxt : %.mxt
	$(PREPARSE) $< > $@
