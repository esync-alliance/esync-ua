#!/usr/bin/env python
import os
import subprocess
import shutil
from optparse import OptionParser
from pylibua.esyncua import eSyncUA


class SampleUA(eSyncUA):
    """Sample Update Agent """

    def do_get_version(self, packageName):
        return eSyncUA.do_get_version(self, packageName)

    def do_prepare_install(self, packageName, version, packageFile):
        print("UA:do_prepare_install %s:%s:%s" %
              (packageName, version, packageFile))
        newFile = os.path.join(self.cache, os.path.basename(packageFile))
        try:
            print("UA: copying %s to %s" % (packageFile, newFile))
            shutil.copy(packageFile, newFile)
            return ['INSTALL_READY', newFile]
        except:
            print("copyfile error!")
            return ['INSTALL_FAILED']

    def do_transfer_file(self, packageName, version, packageFile):
        if self.ssh_user is None:
            print("UA:do_transfer_file")
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

    def do_install(self, downloadFileStr):
        return 'INSTALL_COMPLETED'


if __name__ == "__main__":
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
    parser.add_option('-D', '--delta', default=False, action='store_true', dest="disable_delta",
                    help="disable delta")
    parser.add_option('-d', '--debug', default=False, action='store_true',
                    help="show debug messages")
    (options, args) = parser.parse_args()

    host_p = 'tcp://' + options.host + ':' + str(options.port)
    sample_ua = SampleUA(cert_dir=options.cert_dir,
                       ua_nodeType=options.node_type,
                       host_port=host_p,
                       delta_cap=options.cap,
                       enable_delta=(options.disable_delta is False),
                       debug=options.debug)

    sample_ua.ssh_host = options.host
    sample_ua.ssh_user = options.ssh_user
    sample_ua.ssh_pw = options.ssh_pw
    sample_ua.cache = options.cache
    sample_ua.run_forever()