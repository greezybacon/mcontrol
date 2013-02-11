PREPARSE=python $(PWD)/mcodepp.py

MOTORS=mAxis.mxt nAxis.mxt oAxis.mxt pAxis.mxt uAxis.mxt vAxis.mxt \
	xAxis.mxt yAxis.mxt zAxis.mxt
BUILD=$(MOTORS:%=../../$(BUILDDIR)/dosis-1.7/%)

.PHONY: all
all: build-dir $(BUILD)

.PHONY: build-dir
build-dir:
	mkdir -p ../../$(BUILDDIR)/dosis-1.7

mAxis.mxt nAxis.mxt oAxis.mxt pAxis.mxt: mdrive_base.mxt

../../$(BUILDDIR)/dosis-1.7/%.mxt : %.mxt
	$(PREPARSE) $< > $@
