#!/usr/bin/env python
import os
import subprocess
import shutil
import sys
from optparse import OptionParser
from pylibua.esyncua import eSyncUA


class SampleUA(eSyncUA):
    """Sample Update Agent """

    def do_get_version(self, packageName):
        return eSyncUA.do_get_version(self, packageName)

    def do_prepare_install(self, packageName, version, packageFile):
        print("PUA: do_prepare_install %s:%s:%s" %
              (packageName, version, packageFile))
        try:
            self.newFile = self.cache + os.sep + os.path.basename(packageFile)
            try:
                os.remove(self.newFile)
            except:
                pass
            print("PUA: copying %s to %s" % (packageFile, self.newFile))
            shutil.copy(packageFile, self.newFile)
            return ['INSTALL_READY', self.newFile]
        except Exception as why:
            print("copy error: <%s>" % (str(why)))
            return ['INSTALL_FAILED']

    def do_transfer_file(self, packageName, version, packageFile):
        print("PUA: do_transfer_file")
        if self.ssh_user is None:
            status = 0
        else:
            try:
                remote_path = self.ssh_user + '@' + self.ssh_host + ':' + packageFile
                local_dir = os.path.dirname(packageFile)
                if(self.ssh_pw is None):
                    subprocess.check_call(
                        ['scp', '-o', 'StrictHostKeyChecking no', remote_path, local_dir])
                else:
                    subprocess.check_call(
                        ['sshpass', '-p', self.ssh_pw, 'scp', '-o', 'StrictHostKeyChecking no', remote_path, local_dir])
                status = 0

            except (subprocess.CalledProcessError, OSError) as e:
                print(e)
                status = 1

        return [status]

    def do_install(self, version, packageFile):
        print("PUA: do_install version: %s with %s" % (version, packageFile))
        return 'INSTALL_COMPLETED'


if __name__ == "__main__":

    print("Running: %s" % ' '.join(sys.argv[0:]))
    parser = OptionParser(usage="usage: %prog [options]", version="%prog 1.0")
    parser.add_option("-k", "--cert", default='/data/sota/certs/rom-ua/', type='string',
                      action="store", dest="cert_dir", help="certificate directory", metavar="CERT")
    parser.add_option("-t", "--type", default='/ECU/ROM', type='string',
                      action="store", dest="node_type", help="handler type", metavar="TYPE")
    parser.add_option("-i", "--host", default='localhost', type='string',
                      action="store", dest="host", help="host (localhost)")
    parser.add_option("-p", "--port", default=9133, type='int',
                      action="store", dest="port", help="port (9133)")
    parser.add_option("-u", "--user", type='string', action="store",
                      dest="ssh_user", help="ssh user name ", metavar="USER")
    parser.add_option("-w", "--pass", type='string', action="store",
                      dest="ssh_pw", help="ssh password ", metavar="PASS")
    parser.add_option("-a", "--cap", default='A:3;B:3;C:100', type='string', action="store",
                      help="delta capability ", metavar=" CAP")
    parser.add_option("-c", "--temp", default='/tmp', type='string', action="store",
                      dest="cache", help="cache directory ", metavar="TEMP")
    parser.add_option('-l', '--load', default=False, action='store_true', dest="ready_download",
                      help="enable  DOWNLOAD_CONSENT to ready-download")
    parser.add_option('-D', '--delta', default=False, action='store_true', dest="disable_delta",
                      help="disable delta")
    parser.add_option('-d', '--debug', default=3, action='store', type='int',
                      help="debug level(3), 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG", metavar="LVL")
    (options, args) = parser.parse_args()

    host_p = 'tcp://' + options.host + ':' + str(options.port)
    sample_ua = SampleUA(cert_dir=options.cert_dir,
                         ua_nodeType=options.node_type,
                         host_port=host_p,
                         delta_cap=options.cap,
                         enable_delta=(options.disable_delta is False),
                         debug=options.debug,
                         ready_download=options.ready_download)

    sample_ua.ssh_host = options.host
    sample_ua.ssh_user = options.ssh_user
    sample_ua.ssh_pw = options.ssh_pw
    sample_ua.cache = options.cache
    sample_ua.run_forever()
