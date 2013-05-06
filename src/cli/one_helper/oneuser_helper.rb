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

require 'one_helper'
require 'one_helper/onequota_helper'

class OneUserHelper < OpenNebulaHelper::OneHelper
    def self.rname
        "USER"
    end

    def self.conf_file
        "oneuser.yaml"
    end

    def self.password_to_str_desc
        "User password"
    end

    def self.password_to_str(arg, options)
        if options[:read_file]
            begin
                password = File.read(arg).split("\n").first
            rescue
                return -1, "Cannot read file: #{arg}"
            end
        else
            password = arg.dup
        end

        if options[:driver] == OpenNebula::User::X509_AUTH
            require 'opennebula/x509_auth'
            password = OpenNebula::X509Auth.escape_dn(password)
        end

        if options[:sha1] || options[:driver] == OpenNebula::User::CIPHER_AUTH
            require 'digest/sha1'
            password = Digest::SHA1.hexdigest(password)
        end

        return 0, password
    end

    def password(options)
        case options[:driver]
        when OpenNebula::User::SSH_AUTH
            if !options[:key]
                return -1, "You have to specify the --key option"
            end

            require 'opennebula/ssh_auth'

            begin
                auth = OpenNebula::SshAuth.new(:private_key=>options[:key])
            rescue Exception => e
                return -1, e.message
            end
        when OpenNebula::User::X509_AUTH
            options[:cert] ||= ENV['X509_USER_CERT']

            if !options[:cert]
                return -1, "You have to specify the --cert option"
            end

            require 'opennebula/x509_auth'

            begin
                cert = [File.read(options[:cert])]
                auth = OpenNebula::X509Auth.new(:certs_pem=>cert)
            rescue Exception => e
                return -1, e.message
            end
        else
            return -1, "You have to specify an Auth method or define a password"
        end

        return 0, auth.password
    end

    def self.login(username, options)
        case options[:driver]
        when OpenNebula::User::SSH_AUTH
            require 'opennebula/ssh_auth'

            options[:key]  ||= ENV['HOME']+'/.ssh/id_rsa'

            begin
                auth = OpenNebula::SshAuth.new(:private_key=>options[:key])
            rescue Exception => e
                return -1, e.message
            end
        when OpenNebula::User::X509_AUTH
            require 'opennebula/x509_auth'

            options[:cert] ||= ENV['X509_USER_CERT']
            options[:key]  ||= ENV['X509_USER_KEY']

            begin
                certs = [File.read(options[:cert])]
                key   = File.read(options[:key])

                auth = OpenNebula::X509Auth.new(:certs_pem=>certs, :key_pem=>key)
            rescue Exception => e
                return -1, e.message
            end
        when OpenNebula::User::X509_PROXY_AUTH
            require 'opennebula/x509_auth'

            options[:proxy] ||= ENV['X509_PROXY_CERT']

            begin
                proxy = File.read(options[:proxy])

                certs = proxy.scan(/(-+BEGIN CERTIFICATE-+\n[^-]*\n-+END CERTIFICATE-+)/)
                certs.flatten!

                rc = proxy.match(/(-+BEGIN RSA PRIVATE KEY-+\n[^-]*\n-+END RSA PRIVATE KEY-+)/)
                key= rc[1]

                auth = OpenNebula::X509Auth.new(:certs_pem=>certs, :key_pem=>key)
            rescue => e
                return -1, e.message
            end
        else
            return -1, "You have to specify an Auth method"
        end

        options[:time] ||= 3600

        auth.login(username, Time.now+options[:time])

        return 0, 'export ONE_AUTH=' << auth.class::LOGIN_PATH
    end

    def format_pool(options)
        config_file = self.class.table_conf

        prefix = '/USER_POOL/DEFAULT_USER_QUOTAS/'
        user_pool = @user_pool

        table = CLIHelper::ShowTable.new(config_file, self) do
            column :ID, "ONE identifier for the User", :size=>4 do |d|
                d["ID"]
            end

            column :NAME, "Name of the User", :left, :size=>15 do |d|
                d["NAME"]
            end

            column :GROUP, "Group of the User", :left, :size=>10 do |d|
                helper.group_name(d, options)
            end

            column :AUTH, "Auth driver of the User", :left, :size=>8 do |d|
                d["AUTH_DRIVER"]
            end

            column :VMS , "Number of VMS", :size=>9 do |d|             
                if d.has_key?('VM_QUOTA') and d['VM_QUOTA'].has_key?('VM')
                    limit = d['VM_QUOTA']['VM']["VMS"]

                    if limit == "-1"
                        limit = user_pool["#{prefix}VM_QUOTA/VM/VMS"]
                        limit = "0" if limit.nil? || limit == ""
                    end

                    "%3d / %3d" % [d['VM_QUOTA']['VM']["VMS_USED"], limit]
                else
                    "-"
                end 
            end

            column :MEMORY, "Total memory allocated to user VMs", :size=>17 do |d|
                if d.has_key?('VM_QUOTA') and d['VM_QUOTA'].has_key?('VM')
                    limit = d['VM_QUOTA']['VM']["MEMORY"]

                    if limit == "-1"
                        limit = user_pool["#{prefix}VM_QUOTA/VM/MEMORY"]
                        limit = "0" if limit.nil? || limit == ""
                    end

                    d['VM_QUOTA']['VM']['MEMORY_USED']
                    "%7s / %7s" % [OpenNebulaHelper.unit_to_str(d['VM_QUOTA']['VM']["MEMORY_USED"].to_i,{},"M"),
                    OpenNebulaHelper.unit_to_str(limit.to_i,{},"M")]
                else
                    "-"
                end
            end

            column :CPU, "Total CPU allocated to user VMs", :size=>11 do |d|
                if d.has_key?('VM_QUOTA') and d['VM_QUOTA'].has_key?('VM')
                    limit = d['VM_QUOTA']['VM']["CPU"]

                    if limit == "-1"
                        limit = user_pool["#{prefix}VM_QUOTA/VM/CPU"]
                        limit = "0" if limit.nil? || limit == ""
                    end

                    "%3.1f / %3.0f" % [d['VM_QUOTA']['VM']["CPU_USED"], limit]
                else
                    "-"
                end
            end

            column :PASSWORD, "Password of the User", :size=>50 do |d|
                d['PASSWORD']
            end

            default :ID, :NAME, :GROUP, :AUTH, :VMS, :MEMORY, :CPU
        end

        table
    end

    private

    def factory(id=nil)
        if id
            OpenNebula::User.new_with_id(id, @client)
        else
            xml=OpenNebula::User.build_xml
            OpenNebula::User.new(xml, @client)
        end
    end

    def factory_pool(user_flag=-2)
        #TBD OpenNebula::UserPool.new(@client, user_flag)
        @user_pool = OpenNebula::UserPool.new(@client)
        return @user_pool
    end

    def format_resource(user, options = {})
        system = System.new(@client)

        str="%-15s: %-20s"
        str_h1="%-80s"

        CLIHelper.print_header(str_h1 % "USER #{user['ID']} INFORMATION")
        puts str % ["ID",          user.id.to_s]
        puts str % ["NAME",        user.name]
        puts str % ["GROUP",       user['GNAME']]
        puts str % ["PASSWORD",    user['PASSWORD']]
        puts str % ["AUTH_DRIVER", user['AUTH_DRIVER']]

        puts str % ["ENABLED",
            OpenNebulaHelper.boolean_to_str(user['ENABLED'])]

        puts

        CLIHelper.print_header(str_h1 % "USER TEMPLATE",false)
        puts user.template_str

        user_hash = user.to_hash

        default_quotas = nil

        user.each('/USER/DEFAULT_USER_QUOTAS') { |elem|
            default_quotas = elem
        }

        helper = OneQuotaHelper.new
        helper.format_quota(user_hash['USER'], default_quotas)
    end
end
