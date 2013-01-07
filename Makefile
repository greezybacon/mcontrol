DIRS = lib drivers daemon

.PHONY: all
all:
	set -e; for d in $(DIRS); do $(MAKE) -C $$d all; done

.PHONY: clean
clean:
	set -e; for d in $(DIRS); do $(MAKE) -C $$d clean; done

