all: p2 p3
.PHONY : all

p2:
	python ../setup.py build_ext  -i

p3:
	python3 ../setup.py build_ext  -i

clean:
	rm -rf build libuamodule.py  *.so libuac_wrap.c *.pyc __pycache__