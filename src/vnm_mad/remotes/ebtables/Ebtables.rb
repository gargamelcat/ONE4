# -------------------------------------------------------------------------- #
# Copyright 2002-2013, OpenNebula Project (OpenNebula.org), C12G Labs        #
#                                                                            #
# Licensed under the Apache License, Version 2.0 (the "License"); you may    #
# not use this file except in compliance with the License. You may obtain    #
# a copy of the License at                                                   #
#                                                                            #
# http://www.apache.org/licenses/LICENSE-2.0                                 #
#                                                                            #
# Unless required by applicable law or agreed to in writing, software        #
# distributed under the License is distributed on an "AS IS" BASIS,          #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   #
# See the License for the specific language governing permissions and        #
# limitations under the License.                                             #
#--------------------------------------------------------------------------- #

require 'OpenNebulaNetwork'

class EbtablesVLAN < OpenNebulaNetwork
    DRIVER = "ebtables"

    XPATH_FILTER = "TEMPLATE/NIC[VLAN='YES']"

    def initialize(vm, deploy_id = nil, hypervisor = nil)
        super(vm,XPATH_FILTER,deploy_id,hypervisor)
        @locking = true
    end

    def ebtables(rule)
        OpenNebula.exec_and_log("#{COMMANDS[:ebtables]} -A #{rule}")
    end

    # Activates ebtables rules
    #
    def activate
        lock

        process do |nic|
            tap = nic[:tap]
            if tap
                iface_mac = nic[:mac]

                mac     = iface_mac.split(':')
                mac[-1] = '00'

                net_mac = mac.join(':')

                in_rule="FORWARD -s ! #{net_mac}/ff:ff:ff:ff:ff:00 " <<
                        "-o #{tap} -j DROP"
                out_rule="FORWARD -s ! #{iface_mac} -i #{tap} -j DROP"

                ebtables(in_rule)
                ebtables(out_rule)
            end
        end

        unlock

        return 0
    end

    def deactivate
        lock

        process do |nic|
            mac = nic[:mac]
            # remove 0-padding
            mac = mac.split(":").collect{|e| e.hex.to_s(16)}.join(":")

            tap = ""
            rules.each do |rule|
                if m = rule.match(/#{mac} -i (\w+)/)
                    tap = m[1]
                    break
                end
            end
            remove_rules(tap)
        end

        unlock

        return 0
    end

    def rules
        `#{COMMANDS[:ebtables]} -L FORWARD`.split("\n")[3..-1]
    end

    def remove_rules(tap)
        rules.each do |rule|
            if rule.match(tap)
                remove_rule(rule)
            end
        end
    end

    def remove_rule(rule)
        OpenNebula.exec_and_log("#{COMMANDS[:ebtables]} -D FORWARD #{rule}")
    end
end
