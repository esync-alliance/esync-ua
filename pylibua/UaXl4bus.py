"""

"""
from libuamodule import *
import os
import json
import shutil
import time


def toUtf8(input):
    """ Convert all unicode strings in a JSON object string (input) to UTF-8 strings. 
    Args:
            input(str): JSON object string. 

    Returns:
            JSON object string coded in UTF-8. 
    """
    if isinstance(input, dict):
        return {toUtf8(key): toUtf8(value) for key, value in input.iteritems()}
    elif isinstance(input, list):
        return [toUtf8(element) for element in input]
    elif isinstance(input, unicode):
        return input.encode('utf-8')
    else:
        return input


class UaXl4bus:
    """Base class of update agent via Xl4bus """

    __XL4BUS_OTA_STATUS = ('INSTALL_PENDING', 'INSTALL_IN_PROGRESS',
                           'INSTALL_COMPLETED', 'INSTALL_ABORTED',
                           'INSTALL_ROLLBACK', 'INSTALL_FAILED')

    __signalHandler = {}

    def __init__(self, cert_label, cert_dir, conf_file, ua_nodeType,
                 host_port='tcp://localhost:9133',
                 version_dir='/data/sota/versions',
                 enable_delta=True,
                 use_installation_thread=True,
                 reconstruct_delta=True,
                 force_demo_download=False):
        """Class Constructor 
        Args:
                cert_label(str): Label name of update agent certificate.
                cert_dir(str): Top directory of certificates(ca, broker, and ua).
                version_dir(str): Location of version file for do_set_version() 
                        (default is /data/sota/versions).  
                host_port(str): Host url and port number
                        (Default is tcp://localhost:9133).
                enable_delta(bool): True to enable, False to disable delta feature
                        (Default is True). 
                use_installation_thread(bool): True to run do_install() in a 
                        separate thread (Default is True).
                reconstruct_delta(bool):: If True, this library reconstructs the
                        package with downloaded delta file, the reconstructed file path 
                        is passed in do_install() to UA. 
                        If False,  the path name of the downloaded delta file is passed
                        to UA. UA is responsible for reconstruction. 
                        (Default is True)
                force_demo_download(bool):: For demo purpose, set it to True to 
                        force downloading package even it's available locally. 
                        (Default is False)       

        Returns:
                None
        """
        self.cert_label = cert_label
        self.cert_dir = cert_dir
        self.nodeType = ua_nodeType
        self.version_dir = version_dir
        self.__loadUaConf(conf_file)
        self.to_send_diagnostics_message = False
        self.incremental_failed = False
        self.enable_delta = enable_delta
        self.host_port = host_port
        self.use_installation_thread = use_installation_thread
        self.xl4bus_client_initialized = False
        self.reconstruct_delta = reconstruct_delta
        self.force_demo_download = force_demo_download
        self.libua_debug = 0
        cb = py_ua_cb_t()
        cb.ua_get_version = "do_get_version"
        cb.ua_set_version = "do_set_version"
        cb.ua_pre_install = "do_pre_install"
        cb.ua_install = "do_post_install"
        cb.ua_post_install = "do_confirm_download"
        cb.ua_prepare_install =  "do_prepare_install"
        cb.ua_prepare_download =  "do_confirm_download"      
        pua_set_callbacks(self, cb)

    def run_forever(self):
        """ Start communication with DMClient via Xl4bus-broker. 
                Initialize Xl4bus Client if it hasn't been done yet.
                Start a thread to retrieve XL4bus messages from a Queue for further processing. 
        """
        print(self.cert_label, self.cert_dir, self.version_dir)
        uacfg = ua_cfg_t()
        
        uacfg.url = self.host_port
        uacfg.cert_dir = self.cert_dir
        uacfg.cache_dir = "/tmp/esync/"
        uacfg.backup_dir = "/data/esync/backup/"
        uacfg.delta = self.enable_delta
        uacfg.debug = self.libua_debug
        #uacfg.delta_config 
        #uacfg.rw_buffer_size 
        uacfg.reboot_support = 0
        self.do_init()
        if (self.xl4bus_client_initialized == False and pua_start(self.nodeType, uacfg) == 0):
            self.xl4bus_client_initialized = True

    def do_init(self):
        """Interface to allow device specific initialization    

        Subclass shall overwrite this function to customize device 
        initialization before starting the update agent. 
        """
        pass

    def do_confirm_download(self, pkgName, version):
        """Interface to confirm/deny download after UA receives
        xl4.ready-download message

        Subclass shall return one of the status strings
        ("DOWNLOAD_POSTPONED", "DOWNLOAD_CONSENT", "DOWNLOAD_DENIED", "DOWNLOAD_CONSENT") 

        Status will be sent in xl4.update-status to DMClient
        Default is "DOWNLOAD_CONSENT"
        """
        return "DOWNLOAD_CONSENT"

    def do_pre_install(self, downloadFileStr):
        """Interface to prepare for updating after UA receives 
        xl4.ready-update message

        Subclass shall return one of the status strings
         ("INSTALL_COMPLETED", "INSTALL_ABORTED", "INSTALL_ROLLBACK", "INSTALL_FAILED")

        Default is "INSTALL_IN_PROGRESS"
        Status will be sent in xl4.update-status to DMClient
        """
        return "INSTALL_IN_PROGRESS"

    def do_install(self, downloadFileStr):
        """Interface to start updating after do_pre_install()   

        Subclass shall return one of the status strings 
        ("INSTALL_COMPLETED", "INSTALL_ABORTED", "INSTALL_ROLLBACK", "INSTALL_FAILED")

        Default is "INSTALL_COMPLETED"
        Status will be sent in xl4.update-status to DMClient
        """
        return "INSTALL_COMPLETED"

    def do_post_install(self):
        """Interface to invoke additional action after do_install()"""
        pass

    def do_validate_installed_version(self):
        """ Interface to check if the newly installed version is working properly 
        after do_install()

        Subclass shall return True if success and False is failure
        """
        return True

    def do_get_version(self, packageName):
        """Interface to retrieve current version of UA

        Default is to return a string if 'version' is found in file
        version_dir/packageName. If 'version' is not found, return None. 

        Subclass shall return a version string, e.g. "1.0", or None. 
        """
        fileName = self.version_dir + os.sep + packageName
        if not os.path.exists(self.version_dir):
            os.makedirs(self.version_dir)
        try:
            with open(fileName) as ver_file:
                version_info = json.load(ver_file)
                return version_info['version']
        except IOError as e:
            print str(e)
        return None

    def do_set_version(self, packageName, ver, sha256Rcvd, sha256Local, downloaded, packageFile):
        """ Interface to set version information of UA after successful update. 
        Default is to write all information in json format to a local file
        'version_dir/packageName'. Generally, UA should not need to override
        this function. It's strongly advised to also invoke the baseclass 
        implementation of this function (UaXl4bus.do_set_version()) in the 
        subclass overridden function. 

        Args:
                packageName(str): A string for component package name found in 
                        xl4.ready-update message.
                ver(str): A version string found in xl4.ready-update message.
                sha256Rcvd(str): A version string for 'sha-256' found in 
                        xl4.ready-update message.
                sha256Local(str): A version string for 'sha-256' calculated from 
                        the local packageFile.
                downloaded(bool): True if packageFile is in local filesystem, other False. 
                packageFile(str): An absolute path name to the component package 
                        file for installation. 

        Returns:
                None. 
        """
        fileName = self.version_dir + os.sep + packageName
        backupDir = self.backup_dir
        if(os.path.isdir(backupDir) is not True):
            os.mkdir(backupDir)
        backupFile = os.path.join(backupDir, os.path.basename(packageFile))
        if packageFile != backupFile:
            shutil.copyfile(packageFile, backupFile)
        try:
            with open(fileName, 'r+') as ver_file:
                if self.force_demo_download is True:
                    version_info = {}
                    version_info['version'] = ver
                    version_info['version-list'] = {
                        ver: {'downloaded': downloaded, 'sha-256': sha256Rcvd, "file": backupFile}}

                else:
                    version_info = json.load(ver_file)
                    version_info['version'] = ver
                    version_info['version-list'][ver] = {
                        'downloaded': downloaded, 'sha-256': sha256Rcvd, "file": backupFile}

                ver_file.seek(0)
                ver_file.truncate()
                json.dump(version_info, ver_file)

        except(IOError):
            with open(fileName, 'w+') as ver_file:
                version_info = {}
                version_info['version'] = ver
                version_info['version-list'] = {
                    ver: {'downloaded': downloaded, 'sha-256': sha256Rcvd, "file": backupFile}}
                json.dump(version_info, ver_file)

    def do_prepare_install(self):
        pass

    def send_update_status(self, update_status, file_downloaded=False):
        """ Send xl4.update-status message to DMClient
        Args:
                update_status(str): one of the status string defined in STATUS. 
                file_downloaded(bool): True to indicate UA has the installation 
                        file, otherwise False.
        Returns:
                None
        """

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
        print str(now), jsonDiagStr
        if(pua_send_message(jsonDiagStr) != 0):
            print "Failed to send message"

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
            self.config = toUtf8(json.load(conf_file))
            if('delta' in self.config and 'delta-cap' in self.config['delta']):
                self.delta = self.config['delta']
                self.delta_cap = self.config['delta']['delta-cap']
            if('backup_dir' in self.config):
                self.backup_dir = self.config['backup_dir']

        if hasattr(self, 'delta_cap') is False:
            self.delta_cap = "A:1;B:2;C:10"

        if hasattr(self, 'backup_dir') is False:
            self.backup_dir = "/data/sota/backup"

    def parse_manifest(self, downloadFileStr):
        """ Extract manifest.xml from installation archive and parse xml file to Dictionary object.

        Args:
                downloadFileStr(str): Package path name containing manifest.xml. 

        Returns:
                A Dictionary object with (key, value) pairs from manifest.xml
        """
        dict = {}
        with zipfile.ZipFile(downloadFileStr, "r").open('manifest.xml') as f:
            xmlRoot = ET.parse(f).getroot()
            for elem in xmlRoot:
                dict[elem.tag] = elem.text
        return dict

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
        if (self.xl4bus_client_initialized == False and pua_start(self.cert_label, self.cert_dir, self.host_port) == 0):
            self.xl4bus_client_initialized = True

        return self.xl4bus_client_initialized
