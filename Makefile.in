INSTALL_USER = dosis
INSTALL_GROUP = dosis

# NOTE: DESTDIR in never set in the makefile, only from the
# outside via command line i.e. make DESTDIR=/some/install/target
# This is for package building
BASE_DIR=/home/dosis
INSTALL_ROOT = $(DESTDIR)$(BASE_DIR)

USR_LIB_ROOT = /usr/lib/
INSTALL_FLAGS = -p # -o $(INSTALL_USER) -g $(INSTALL_GROUP)
BIN_PERMS = -m 0775
LIB_PERMS = -m 0664
DIR_PERMS = -m 0755
DATA_PERMS = -m 0644
CONFIG_PERMS = -m 0664
CRON_PERMS = -m 0755
INSTALL = /usr/bin/install
INSTALL_BIN = ${INSTALL} ${INSTALL_FLAGS} ${BIN_PERMS}
INSTALL_LIB = ${INSTALL} ${INSTALL_FLAGS} ${LIB_PERMS}
INSTALL_DATA = ${INSTALL} ${INSTALL_FLAGS} ${DATA_PERMS}
INSTALL_DIR = ${INSTALL} -d ${INSTALL_FLAGS} ${DIR_PERMS}

SUDO = 

TMPDIR = $(shell pwd)/.rpmbuild
BIN_DIR = $(INSTALL_ROOT)/bin
LIB_DIR = $(INSTALL_ROOT)/lib
OPT_DIR = $(DESTDIR)/opt/dosis/mcontrol
TOOLS_DIR = $(INSTALL_ROOT)/tools
DRIVER_DIR = $(OPT_DIR)/drivers
MCODE_DIR = $(OPT_DIR)/microcode
FIRMWARE_DIR = $(OPT_DIR)/firmware
