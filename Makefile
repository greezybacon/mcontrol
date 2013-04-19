include Makefile.in

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
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(MCODE_DIR)/dosis-1.x
	$(INSTALL) -d $(INSTALL_FLAGS) $(BIN_PERMS) $(FIRMWARE_DIR)

install: install-dirs
	$(INSTALL) $(INSTALL_FLAGS) $(BIN_PERMS) daemon/mcontrold $(BIN_DIR)/
	$(INSTALL) $(INSTALL_FLAGS) $(LIB_PERMS) lib/libmcontrol.so $(LIB_DIR)/
	$(INSTALL) $(INSTALL_FLAGS) $(LIB_PERMS) drivers/*.so $(DRIVER_DIR)/
	$(INSTALL) $(INSTALL_FLAGS) $(CONFIG_PERMS) \
		drivers/mdrive/firmware/*.IMS $(FIRMWARE_DIR)
	$(INSTALL) $(INSTALL_FLAGS) $(CONFIG_PERMS) \
		drivers/mdrive/microcode/build/dosis-1.x/*.mxt \
		$(MCODE_DIR)/dosis-1.x
	$(MAKE) BIN_DIR="$(BIN_DIR)" -C lib/python install

rpm:
	mkdir -p ${TMPDIR}/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
	git archive HEAD . --prefix=mcontrol-$(VERSION_MAJOR)/ \
		> ${TMPDIR}/SOURCES/mcontrol-source.tar
	sed -re s/GIT_VERSION/$(shell git describe)/ \
		mcontrol.spec > ${TMPDIR}/SPECS/mcontrol.spec
	rpmbuild --define "_topdir ${TMPDIR}" \
		-bb ${TMPDIR}/SPECS/mcontrol.spec
	find ${TMPDIR}/RPMS/ -type f -name "*.rpm" -exec mv {} . \;
