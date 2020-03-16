#!/usr/bin/env python
import os
import subprocess
import shutil
import sys
from optparse import OptionParser
from pylibua.esyncua import eSyncUA


class TestUA(eSyncUA):
    """Test Update Agent """

    def init_version(self, version):
        eSyncUA.do_set_version(self, '', version)

    def do_get_version(self, packageName):
        return eSyncUA.do_get_version(self, packageName)

    def do_prepare_install(self, packageName, version, packageFile):
        if(self.mode == 4):
            return ['INSTALL_FAILED']
        else:
            return ['INSTALL_READY']

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

    def do_install(self, version, packageFile):
        print("PUA: do_install version: %s with %s" % (version, packageFile))
        if(self.mode == 1):
            test_ua.install_state =  'INSTALL_FAILED'
        elif(self.mode == 2):
            if(test_ua.install_state == 'INSTALL_COMPLETED'):
                test_ua.install_state = 'INSTALL_FAILED'
            else:
                test_ua.install_state = 'INSTALL_COMPLETED'
        elif(self.mode == 3):
            print("testing rollback")
            if(version == test_ua.rb_ver):
                test_ua.install_state = 'INSTALL_COMPLETED'
            else:
                test_ua.install_state = 'INSTALL_FAILED'
        else:
            test_ua.install_state = 'INSTALL_COMPLETED'

        print("PUA: test mode %d: returns %s" % (test_ua.mode, test_ua.install_state))
        return test_ua.install_state


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
    parser.add_option("-v", "--tver", type='string', action="store",
                      dest="ua_ver", help="initial test UA version string", metavar="TVER")
    parser.add_option("-M", "--mode", type='int', action="store",
                      dest="mode", default=0, help="update mode: 0: success(default); 1: fail; 2: toggle(fail/success); 3: rollback; 4: prepare-update failed", metavar="MODE")
    parser.add_option("-r", "--rver", type='string', action="store",
                      dest="rb_ver", help="rollback test version string", metavar="RVER")
    parser.add_option('-D', '--delta', default=False, action='store_true', dest="disable_delta",
                    help="disable delta")
    parser.add_option('-d', '--debug', default=False, action='store_true',
                    help="show debug messages")
    (options, args) = parser.parse_args()

    host_p = 'tcp://' + options.host + ':' + str(options.port)
    test_ua = TestUA(cert_dir=options.cert_dir,
                       ua_nodeType=options.node_type,
                       host_port=host_p,
                       delta_cap=options.cap,
                       enable_delta=(options.disable_delta is False),
                       debug=options.debug)

    test_ua.ssh_host = options.host
    test_ua.ssh_user = options.ssh_user
    test_ua.ssh_pw = options.ssh_pw
    test_ua.mode = options.mode
    test_ua.install_state = 'INSTALL_COMPLETED'
    test_ua.rb_ver = options.rb_ver
    test_ua.init_version(options.ua_ver)
    if(test_ua.mode == 3 and test_ua.rb_ver is None):
        print("Trying rollback test, please set final rollback version with -r option.")
        sys.exit()
    test_ua.run_forever()