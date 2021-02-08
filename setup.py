#!/usr/bin/env python3

"""
setup.py file for libuamodule
"""
import os
from setuptools import setup, Extension

libua_dir = os.path.dirname(os.path.realpath(__file__))
libua_build = os.getenv("LIBUA_BUILD")
if(libua_build is not None and len(libua_build) == 0):
    libua_build = None

libua_api_ver = os.getenv("LIBUA_API_VER")
if(libua_api_ver is not None and len(libua_api_ver) == 0):
    libua_api_ver = None
if(libua_api_ver is not None):
    libua_name = 'ua_2'
else:
    libua_name = 'ua'

xl4bus_build = os.getenv("XL4BUS_DIR")
if(xl4bus_build is not None and len(xl4bus_build) == 0):
    xl4bus_build = None
	
if(xl4bus_build is not None):
    if('~' in xl4bus_build):
        xl4bus_build = os.path.expanduser(xl4bus_build)
    pylib_inc = [os.path.join(libua_dir, 'src/include'),
                 os.path.join(xl4bus_build, '../src/include'),
                 os.path.join(xl4bus_build, 'include')]
else:
    pylib_inc = [os.path.join(libua_dir, 'src/include'), libua_build]

moduleconf = {
    'name': '_libuamodule',
    'sources': [os.path.join(libua_dir, 'pylibua/libuac.i'),
                os.path.join(libua_dir, 'pylibua/pua.c')],
    'libraries': [libua_name, 'xl4bus', 'xml2', 'zip'],
    'include_dirs': pylib_inc,
    'extra_compile_args' : ["-Wall", "-Werror"]
}
moduleconf['library_dirs'] = []
moduleconf['runtime_library_dirs'] = []
if(xl4bus_build is not None):
    moduleconf['library_dirs'].append(xl4bus_build)
    moduleconf['runtime_library_dirs'].append(xl4bus_build)

if(libua_build is not None):
    moduleconf['include_dirs'].append(libua_build)
    moduleconf['library_dirs'].append(libua_build)
    moduleconf['runtime_library_dirs'].append(libua_build)

if(libua_api_ver is not None):
    moduleconf['extra_compile_args'].append("-DLIBUA_VER_2_0")

esyncua_module = Extension(**moduleconf)

setup(name='esync-libua',
      version='1.0',
      description=""" eSync Python Update Agent Library Using libua.so """,
      author='Excelfore',
      author_email='support@excelfore.com',
      url='https://excelfore.com/',
      ext_modules=[esyncua_module],
      package_dir={'pylibua': os.path.join(libua_dir, 'pylibua')},
      packages=['pylibua']
      )