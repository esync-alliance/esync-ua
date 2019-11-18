# README for Python Update Agent Library

## Scope

This README describes the following:

    - Build the Python update agent library.
    - Run the sample Python UA.
    - Develop a new Python UA.

## Pre-requisites

1. Download and build libxl4bus
1. Download and build updateagent-c (i.e. This project, which includes both C & Python libraries)
1. Install swig package
    sudo apt install swig

## Build Instructions

- Please use Python setuptools to build and install the libary.

    ```python
    cd ..

    #Build everything.
    XL4BUS_DIR=/your/path/to/libxl4bus/build python3 setup.py build_ext

    #Intsall to Python 3 system package directories.
    python3 setup.py install

    # Convenient for development, install/uninstall to user package directories.
    python3 setup.py develop --user
    python3 setup.py develop --user -u
    ```

- Alternatively, limited support is to build Python library is available with c library (libua) CMAKE. please refer to ../README. As of now, CMake tries to search for system path of Python binary. If found, it invokes setuptools. Please make sure these settings are added to ../linux_port/config.cmk

        set(BUILD_PY_LIBUA true)
        set(XL4BUS_DIR /your/path/to/libxl4bus/build)

## Running Python Sample UA

1. eSync Python UA Library Files:

    - _libuamodule.so - C extension shared library used by Python 2 UA library.
    - _libuamodule.cpython-35m-x86_64-linux-gnu.so, C library used by Python 3 UA library.
    - libuamodule.py, module interface to call C extension.
    - esyncua.py - Base class eSync Update Agent.  
    - tmpl_ua.py - Template Python Update Agent, serves as  a simple reference UA implementation.  

2. Provision eSync related data:

    - Prepare dm_tree in /data/sota/dm_tree
    - Preprare node certs in /data/sota/certs/
    - the sample UA uses node_type='/ECU/ROM' as default UA handler type, located in /data/sota/certs/rom-ua
    - Upload eSync component with handler type of '/ECU/ROM' to eSync server.
    - Deploy a compaign to update the component targeted to the VIN in dm_tree.

3. Start DMclient and xl4broker (and policy agent as needed):

    ```bash
    dmclient -t /data/sota/dm_tree
    xl4bus-broker -k /data/sota/certs/broker/private.pem -c /data/sota/certs/broker/cert.pem -t /data/sota/certs/ca/ca.pem
    ```

4. Run 'python tmpl_ua.py', or  'python3 tmpl_ua.py' if Python 3 is used.

As a result, a corresponding package file should be downloaded to the default location (/data/sota/tmp/)

## Development Quick Start Guides

1. To write a new update agent, a subclass of eSyncUA should be implemented like SampleUA in tmpl_ua.py. The following code snippet prints the pathname of the downloaded package. Most UA extracts contents in this package for target installation.

    ```python
    from pylibua.esyncua import eSyncUA

    class SampleUA(eSyncUA):
        def do_install(self, downloadFileStr):
           return "INSTALL_COMPLETED"
    ```

1. Implement one or more of the available interfaces defined by the base class (eSyncUA):

    ```python
    do_init()
    do_get_version()
    do_set_version()
    do_confirm_download()
    do_transfer_file()
    do_prepare_install()
    do_pre_install()
    do_install()
    do_post_install()
    ```

1. Create an instance of the subclass and call run_forever() function to invoke the new update agent.

    ```python
    sample_ua = SampleUA(cert_dir=options.cert_dir,
                       ua_nodeType=options.node_type,
                       host_port=host_p,
                       delta_cap=options.cap,
                       enable_delta=(options.disable_delta is False),
                       debug=options.debug)
    sample_ua.run_forever()
    ```

1. That's it! You can run the new update agent like 'python new_ua.py'.
