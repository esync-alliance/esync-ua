"""

"""
import libuamodule
import os
import json
import time
import logging
import sys
from collections import OrderedDict


class UaXl4bus:
    """Base class of update agent via Xl4bus """

    __XL4BUS_OTA_STATUS = ('INSTALL_PENDING', 'INSTALL_IN_PROGRESS',
                           'INSTALL_COMPLETED', 'INSTALL_ABORTED',
                           'INSTALL_ROLLBACK', 'INSTALL_FAILED')

    def __init__(self, cert_dir, conf_file, ua_nodeType,
                 host_port='tcp://localhost:9133',
                 version_dir='/data/sota/versions',
                 enable_delta=True,
                 reconstruct_delta=True):
        """Class Constructor 
        Args:
                cert_dir(str): Top directory of UA certificates.
                version_dir(str): Location of version file for do_set_version() 
                        (default is /data/sota/versions).  
                host_port(str): Host url and port number
                        (Default is tcp://localhost:9133).
                enable_delta(bool): True to enable, False to disable delta feature
                        (Default is True). 
                reconstruct_delta(bool):: If True, this library reconstructs the
                        package with downloaded delta file, the reconstructed file path 
                        is passed in do_install() to UA. 
                        If False,  the path name of the downloaded delta file is passed
                        to UA. UA is responsible for reconstruction. 
                        (Default is True)      

        Returns:
                None
        """
        self.cert_dir = cert_dir
        self.nodeType = ua_nodeType
        self.version_dir = version_dir
        self.__loadUaConf(conf_file)
        self.enable_delta = enable_delta
        self.host_port = host_port
        self.xl4bus_client_initialized = False
        self.reconstruct_delta = reconstruct_delta
        self.libua_debug = 0
        self.ua_version = None

        cb = libuamodule.py_ua_cb_t()
        if self.do_pre_install.__code__ is not UaXl4bus.do_pre_install.__code__:
            cb.ua_pre_install = "do_pre_install"
        if self.do_post_install.__code__ is not UaXl4bus.do_post_install.__code__:
            cb.ua_post_install = "do_post_install"
        if self.do_prepare_install.__code__ is not UaXl4bus.do_prepare_install.__code__:
            cb.ua_prepare_install = "do_prepare_install"
        if self.do_transfer_file.__code__ is not UaXl4bus.do_transfer_file.__code__:
            cb.ua_transfer_file = "do_transfer_file"
        if self.do_dmc_presence.__code__ is not UaXl4bus.do_dmc_presence.__code__:
            cb.ua_dmc_presence = "do_dmc_presence"
        if self.do_message.__code__ is not UaXl4bus.do_message.__code__:
            cb.ua_on_message = "do_message"

        cb.ua_get_version = "do_get_version"
        cb.ua_set_version = "do_set_version"
        cb.ua_install = "do_install"
        cb.ua_prepare_download = "do_confirm_download"
        libuamodule.pua_set_callbacks(self, cb)

    def run_forever(self):
        """ Start communication with DMClient via Xl4bus-broker. 
                Initialize Xl4bus Client if it hasn't been done yet.
                Start a thread to retrieve XL4bus messages from a Queue for further processing. 
        """
        print(self.cert_dir, self.version_dir)
        uacfg = libuamodule.ua_cfg_t()

        uacfg.url = self.host_port
        uacfg.cert_dir = self.cert_dir
        uacfg.cache_dir = "/tmp/esync/"
        uacfg.backup_dir = "/data/sota/esync/"
        uacfg.delta = self.enable_delta
        uacfg.debug = self.libua_debug
        uacfg.reboot_support = 0
        self.do_init()
        if (self.xl4bus_client_initialized == False and libuamodule.pua_start(self.nodeType, uacfg) == 0):
            self.xl4bus_client_initialized = True

    def do_init(self):
        """
        [Optional] Interface to allow device specific initialization, this is
        called before initializing xl4bus.

        Subclass shall overwrite this function to customize device 
        initialization before starting the update agent. 
        """
        pass

    def do_confirm_download(self, pkgName, version):
        """
        [Optional] Interface to confirm/deny download after UA receives
        xl4.ready-download message        

        Args:
            pkgName: Component package name.
            version: version string.

        Returns:
            Subclass shall return one of the status strings
            ("DOWNLOAD_POSTPONED", "DOWNLOAD_DENIED", "DOWNLOAD_CONSENT")
            Default is "DOWNLOAD_CONSENT"
        """
        return "DOWNLOAD_CONSENT"

    def do_pre_install(self, downloadFileStr):
        """
        [Optional] Interface to prepare for updating after UA receives 
        xl4.ready-update message

        Args:
            pkgName: Component package name.
            version: version string.

        Returns:
            Subclass shall return one of the status strings
            ("INSTALL_IN_PROGRESS", "INSTALL_FAILED")
            Default is "INSTALL_IN_PROGRESS"
        """
        return "INSTALL_IN_PROGRESS"

    def do_install(self, downloadFileStr):
        """
        [Required] Interface to start updating after do_pre_install() upon
        receiving xl4.ready-update message.  

        Args:
            downloadFileStr: Full pathname of the installation package file.

        Returns:
            Subclass shall return one of the status strings 
            ("INSTALL_COMPLETED", "INSTALL_FAILED")
            Default is "INSTALL_COMPLETED"
        """
        return "INSTALL_COMPLETED"

    def do_post_install(self, packageName):
        """
        [Optional] Interface to invoke additional action after do_install()
        Args:
            packageName: component package name.

        Returns:
            None
        """
        pass

    def do_get_version(self, packageName):
        """
        [Optional]  Interface to retrieve current version of UA

        Args:
            packageName: component package name.

        Returns:
            list of two items [status, "version"]
            status(int): 0 for success, 1 for error.
            version(str): Version string.


            Subclass shall a list in the for [0, "v1.0"]. 
        """
        return [0, self.ua_version]

    def do_set_version(self, packageName, ver):
        """ 
        [Optional] Interface to set version of UA after successful update.

        Args:
            packageName(str): A string for component package name found in 
                              xl4.ready-update message.
            ver(str): A version string found in xl4.ready-update message.

        Returns:
                int: 0 for success, 1 for error.
        """
        self.ua_version = ver
        return 0

    def do_prepare_install(self, packageName, version, packageFile):
        """ 
        [Optional] Interface to allow UA to manage 'packageFile' after
        receiving xl4.prepare-update. e.g. A system might need to copy
        'packageFile' to a specific directory. In such case, UA shall
        return the new pathname, which will be passed to do_install.

        Args:
            packageName(str): Component package name.
            version(str): Version string.
            packageFile(str): Full file path of downloaded package file.

        Returns:
            A list of one or two strings ["status", "newPath"].
            status(str): required, one of ("INSTALL_READY", "INSTALL_FAILED")
            newPath(str): optional, only return newPath to inform the new file
                pathname should be used for installation in do_install.
        """
        return ["INSTALL_READY"]

    def do_transfer_file(self, packageName, version, packageFile):
        """ 
        [Optional] Interface to allow UA to transfer 'packageFile' from
        a remote system to the local filesystem for installation, after
        receiving . UA shall return the new local pathname,
        which will be used for further processing. Note that this inteface is
        invoked after receiving xl4.prepare-update, before calling
        do_prepare_install

        Args:
            packageName(str): Component package name.
            version(str): Version string.
            packageFile(str): Full file path of downloaded package file.

        Returns:
            A list of one or two strings [status, "newPath"].
            status(int): required, 0 for success, 1 for error.
            newPath(str): optional, only return newPath to inform the new file
                pathname should be used for further processing.
        """
        return [0]

    def do_dmc_presence(self):
        pass

    def do_message(self):
        pass

    def send_diag_data(self, message, level='INFO', timestamp=None, compoundable=True, nodeType=None):
        """ Send diagnostic message (xl4.log-report) to DMClient
        Args:
                message(str): message JSON object should be created by 
                        DiagnosticsMessage::getMessage()           
                level(str): level for this diagnostics message - EVENT|INFO|WARN|ERROR|SEVERE
                timestamp(float): epoch time (seconds) at which the diagnostic data was captured
                        if no timestamp is given DMClient will attach a timestamp to it
                compoundable(bool): True is compoundable, otherwiese False. 
                nodeType(str): Package handler type, e.g. /ECU/Xl4/

        Returns:
                None
        """
        if nodeType is None:
            nodeType = self.nodeType

        diagMsg = OrderedDict([('type', 'xl4.log-report'), ('body', None)])
        body = OrderedDict([('type', nodeType), ('level', level), ('timestamp', timestamp),
                            ('compoundable', compoundable), ('message', message)])

        diagMsg['body'] = body

        jsonDiagStr = json.dumps(diagMsg)
        now = time.strftime("[%m-%d:%H:%M:%S]", time.localtime())
        print(str(now), jsonDiagStr)
        if(libuamodule.pua_send_message(jsonDiagStr) != 0):
            print("Failed to send message")

    def set_xl4bus_debug(self, enable=0):
        """Enable/Disable xl4bus debug messages. 

        Args:
            enable(int): 0 to disable, 1 to enable xl4bus debug messages

        Returns:
            None. 
        """
        self.libua_debug = enable

    def __loadUaConf(self, filename):
        with open(filename) as conf_file:
            self.config = (json.load(conf_file))
            if('delta' in self.config and 'delta-cap' in self.config['delta']):
                self.delta = self.config['delta']
                self.delta_cap = self.config['delta']['delta-cap']
            if('backup_dir' in self.config):
                self.backup_dir = self.config['backup_dir']

        if hasattr(self, 'delta_cap') is False:
            self.delta_cap = "A:1;B:2;C:10"

        if hasattr(self, 'backup_dir') is False:
            self.backup_dir = "/data/sota/backup"

    def is_xl4bus_initialized(self):
        """ Return if xl4bus client has been initialzed, UA can start sending messages if so. 

        Args:
            None
        Returns:
            True if initialzed, otherwise False.  
        """
        return self.xl4bus_client_initialized

    def initialize_xl4bus_client(self):
        """ Initialize xl4bus client only without starting message processing loop.

        Args:
            None
        Returns:
            True if initialzed, otherwise False.  
        """
        if (self.xl4bus_client_initialized == False and libuamodule.pua_start(self.nodeType, self.cert_dir, self.host_port) == 0):
            self.xl4bus_client_initialized = True

        return self.xl4bus_client_initialized
