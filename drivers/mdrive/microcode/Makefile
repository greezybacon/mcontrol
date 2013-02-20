DIRS=modules/dosis-1.7
BUILDDIR=build

.PHONY: all
all:
	mkdir -p $(BUILDDIR)
	set -e; for d in $(DIRS); do $(MAKE) BUILDDIR=$(BUILDDIR) -C $$d all; done

.PHONY: clean
clean:
	$(RM) -r $(BUILDDIR)
