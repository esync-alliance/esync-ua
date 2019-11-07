#!/usr/bin/env python

"""
setup.py file for libuamodule
"""
import os
from subprocess import check_output
import distutils.sysconfig
from distutils.core import setup, Extension

src_dir =  os.path.dirname(os.path.realpath(__file__))
libua_build = os.path.join(src_dir, 'build')
xl4bus_build = os.getenv("XL4BUS_DIR")
if(xl4bus_build is not None):
	pylib_inc = [os.path.join(libua_build, '../src/include'), 
				libua_build,
				os.path.join(xl4bus_build, '../src/include'),
				os.path.join(xl4bus_build, 'include')]
else:
	pylib_inc = [os.path.join(libua_build, '../src/include'),libua_build]

moduleconf = {
			'name' : '_libuamodule',
			'sources' : [ os.path.join(src_dir, 'pylibua/libuac.i'),
							os.path.join(src_dir, 'pylibua/pua.c') ],
			'libraries' : ['ua', 'xl4bus', 'xml2', 'zip'],
			'include_dirs' : pylib_inc,
			}

if(xl4bus_build is not None):
	moduleconf['library_dirs'] = [xl4bus_build, libua_build]
	moduleconf['runtime_library_dirs'] = [xl4bus_build, libua_build]
esyncua_module = Extension(**moduleconf)

setup (name = 'esync-libua-python',
       version      = '2.0',
       description  = """ eSync Python Update Agent Library Using libua.so """,
       author='Excelfore',
       author_email='support@excelfore.com',
       url='https://excelfore.com/',
       ext_modules  = [esyncua_module],
       packages=['pylibua'],
       )