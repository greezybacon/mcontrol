include include.mk

# Change to 1 to enable debug build
DEBUG = 0

DIRS = lib drivers daemon

.PHONY: all
all:
	set -e; for d in $(DIRS); do $(MAKE) -C $$d all; done

.PHONY: clean
clean:
	set -e; for d in $(DIRS); do $(MAKE) -C $$d clean; done

.PHONY: install install-dirs
install-dirs:
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(BIN_DIR)
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(LIB_DIR)
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(OPT_DIR)
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(TOOLS_DIR)
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(DRIVER_DIR)
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(MCODE_DIR)
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(MCODE_DIR)/dosis-1.7
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(FIRMWARE_DIR)

install: install-dirs
	$(INSTALL) $(INSTALL_FLAGS) $(BIN_PERMS) daemon/mcontrol $(BIN_DIR)/
	$(INSTALL) $(INSTALL_FLAGS) $(LIB_PERMS) lib/libmcontrol.so $(LIB_DIR)/
	$(INSTALL) $(INSTALL_FLAGS) $(LIB_PERMS) drivers/*.so $(DRIVER_DIR)/
	$(INSTALL) $(INSTALL_FLAGS) $(CONFIG_PERMS) \
		drivers/mdrive/firmware/*.IMS $(FIRMWARE_DIR)
	$(INSTALL) $(INSTALL_FLAGS) $(CONFIG_PERMS) \
		drivers/mdrive/microcode/build/dosis-1.7/*.mxt \
		$(MCODE_DIR)/dosis-1.7
	$(MAKE) BIN_DIR="$(BIN_DIR)" -C lib/python install

rpm:
	mkdir -p ${TMPDIR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
	git archive feature/rpm . -o ${TMPDIR}/SOURCES/mcontrol-source.tar \
		--prefix=mcontrol-1.0/
	cp mcontrol.spec ${TMPDIR}/SPECS
	rpmbuild --define "_topdir ${TMPDIR}" \
		-ba ${TMPDIR}/SPECS/mcontrol.spec
	find ${TMPDIR}/RPMS/ -type f -name "*.rpm" -exec mv {} . \;
