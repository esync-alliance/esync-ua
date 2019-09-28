#!/usr/bin/env python
import os
import subprocess
from optparse import OptionParser
from UaXl4bus import UaXl4bus

class MyUaXl4bus(UaXl4bus):
    """Simple update agent """

    def do_get_version(self, packageName):
        return UaXl4bus.do_get_version(self,packageName)

    def do_pre_install(self, downloadFileStr):
        if self.ssh_user is None:
            status = 'INSTALL_IN_PROGRESS'
        else:
            try:
                remote_path = self.ssh_user + '@' + self.ssh_host + ':' + downloadFileStr
                local_dir = os.path.dirname(downloadFileStr)
                if(self.ssh_pw is None):
                    subprocess.check_call(['scp', '-o', 'StrictHostKeyChecking no', remote_path, local_dir])
                else:
                    subprocess.check_call(['sshpass', '-p', self.ssh_pw, 'scp', '-o', 'StrictHostKeyChecking no', remote_path, local_dir])
                status = 'INSTALL_IN_PROGRESS'

            except (subprocess.CalledProcessError, OSError), e:
                print e
                status = 'INSTALL_FAILED'
            
        return status
        
    def do_install(self, downloadFileStr):
       return 'INSTALL_COMPLETED'

if __name__ == "__main__":
    parser = OptionParser(usage="usage: %prog [options]", version="%prog 1.0")    
    parser.add_option("-k", "--cert", default='/data/sota/pki/certs/gateway-mcvp', type='string', action="store", dest="cert_dir", help="certificate directory", metavar="type")
    parser.add_option("-t", "--type", default='/GATEWAY/MCVP', type='string', action="store", dest="node_type", help="handler type", metavar="type")
    parser.add_option("-i", "--host", default='localhost', type='string', action="store", dest="host", help="host (localhost)")
    parser.add_option("-p", "--port", default=9133, type='int', action="store", dest="port", help="port (9133)")
    parser.add_option("-u", "--user", type='string', action="store", dest="ssh_user", help="ssh user name ", metavar="USER")
    parser.add_option("-w", "--pass", type='string', action="store", dest="ssh_pw", help="ssh password ", metavar="PASS")
    (options, args) = parser.parse_args()

    host_p = 'tcp://' + options.host + ':' + str(options.port)

    my_ua = MyUaXl4bus(cert_label='obsolete', cert_dir=options.cert_dir,
                       conf_file='/data/sota/ua/ua.conf', ua_nodeType=options.node_type,
                       host_port=host_p)

    my_ua.set_xl4bus_debug(0)
    my_ua.ssh_host = options.host
    my_ua.ssh_user = options.ssh_user
    my_ua.ssh_pw = options.ssh_pw
    my_ua.run_forever()

