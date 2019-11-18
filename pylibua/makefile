
# This makefile is meant to be used in x86 Linux development enviroment,
# not for cross-build.
# Default is to build for Python 3. 
# To build for Python 3, set the enviroment variable PY2_BUILD
# 	export PY2_BUILD=1

MKFILE_DIR = $(shell (dirname $(abspath $(lastword $(MAKEFILE_LIST)))))
SETUP_PY_DIR = $(shell (realpath "$(MKFILE_DIR)/.."))
BUILD_DIR=$(SETUP_PY_DIR)/build
export XL4BUS_DIR=~/gitlab/libxl4bus/build
export LIBUA_BUILD=${BUILD_DIR}

ifdef PY2_BUILD
	PY = python
	PYDOC = pydoc
else
	PY = python3
	PYDOC = pydoc3
endif

# default build
all: dev

build: libua
	cd $(BUILD_DIR) && $(PY) $(SETUP_PY_DIR)/setup.py build_ext

# build libua.so if not available.
${BUILD_DIR}/libua.so : 
	mkdir -p ${BUILD_DIR} && cd ${BUILD_DIR} && cmake .. && make

# build and install python library to user ~/.local/lib/ for testing.
dev: ${BUILD_DIR}/libua.so
	cd $(SETUP_PY_DIR) && $(PY) $(SETUP_PY_DIR)/setup.py develop --user

# output python document as xml file (esyncua.html)
doc: dev
	cd $(MKFILE_DIR) &&  $(PYDOC) -w ${MKFILE_DIR}/esyncua.py

clean:
	cd $(SETUP_PY_DIR) && $(PY) $(SETUP_PY_DIR)/setup.py develop --user -u

cleanall: clean
	rm -rf $(MKFILE_DIR)/libuamodule.py  $(MKFILE_DIR)/libuac_wrap.c $(MKFILE_DIR)/__pycache__  \
		$(BUILD_DIR)/build $(BUILD_DIR)/*.linux-x86_64* \
		$(SETUP_PY_DIR)/_libuamodule*.so $(SETUP_PY_DIR)/*.egg-info
