WORKDIR=$(shell pwd)
SETUP_PY_DIR=$(WORKDIR)/..

all: p2 p3 doc
.PHONY : all

p2:
	python $(SETUP_PY_DIR)/setup.py build_ext  -i

p3:
	python3 $(SETUP_PY_DIR)/setup.py build_ext  -i

doc:
	pydoc -w $(WORKDIR)/esyncua.py

clean:
	rm -rf build libuamodule.py  *.so libuac_wrap.c *.pyc __pycache__