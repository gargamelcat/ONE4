#include "LibVirtDriver.h"

#include "Nebula.h"
#include <sstream>
#include <fstream>
#include <libgen.h>

int LibVirtDriver::deployment_description_lxc(
        const VirtualMachine *  vm,
        const string&           file_name) const
{
    ofstream                    file;

    int                         num;
    vector<const Attribute *>   attrs;

    string  vcpu;
    string  memory;

    int     memory_in_kb = 0;

    string  emulator_path = "";

    string  kernel     = "";
    string  initrd     = "";
    string  boot       = "";
    string  root       = "";
    string  kernel_cmd = "";
    string  bootloader = "";
    string  arch       = "";

    //const VectorAttribute * disk;
    const VectorAttribute * context;

    string  type       = "";
    string  target     = "";
    string  bus        = "";
    string  ro         = "";
    string  driver     = "";
    string  cache      = "";
    string  default_driver       = "";
    string  default_driver_cache = "";
    //bool    readonly;

    const VectorAttribute * nic;

    string  mac        = "";
    string  bridge     = "";
    string  script     = "";
    string  model      = "";
    string  ip         = "";
    string  filter     = "";
    string  default_filter = "";

    const VectorAttribute * graphics;

    string  listen     = "";
    string  port       = "";
    string  passwd     = "";
    string  keymap     = "";

    const VectorAttribute * input;

    const VectorAttribute * features;

    string     pae     = "";
    string     acpi    = "";

    //const VectorAttribute * raw;
    string data;

    // ------------------------------------------------------------------------

    file.open(file_name.c_str(), ios::out);

    if (file.fail() == true)
    {
        goto error_file;
    }

    // ------------------------------------------------------------------------
    // Starting XML document
    // ------------------------------------------------------------------------

    file << "<domain type='"
         << emulator
         << "' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>"
         << endl;

    // ------------------------------------------------------------------------
    // Domain name
    // ------------------------------------------------------------------------

    file << "\t<name>lxc-" << vm->get_oid() << "</name>" << endl;

    // ------------------------------------------------------------------------
    // CPU
    // ------------------------------------------------------------------------

    vm->get_template_attribute("VCPU", vcpu);

    if(vcpu.empty())
    {
        get_default("VCPU", vcpu);
    }

    if (!vcpu.empty())
    {
        file << "\t<vcpu>" << vcpu << "</vcpu>" << endl;
    }

    // ------------------------------------------------------------------------
    // Memory
    // ------------------------------------------------------------------------

    vm->get_template_attribute("MEMORY",memory);

    if (memory.empty())
    {
        get_default("MEMORY",memory);
    }

    if (!memory.empty())
    {
        memory_in_kb = atoi(memory.c_str()) * 1024;

        file << "\t<memory>" << memory_in_kb << "</memory>" << endl;
    }
    else
    {
        goto error_memory;
    }

    // ------------------------------------------------------------------------
    //  OS options
    // ------------------------------------------------------------------------

    file << "\t<os>" << endl;
    file << "\t\t<type>exe</type>" << endl;
    file << "\t\t<init>/sbin/init</init>" << endl;
    file << "\t</os>" << endl;

    attrs.clear();

    // ------------------------------------------------------------------------
    // Emulator path
    // ------------------------------------------------------------------------
    file << "\t<devices>" << endl;
    emulator_path = "/usr/lib/libvirt/libvirt_lxc";
    file << "\t\t<emulator>" << emulator_path << "</emulator>" << endl;

    // ---- lxc filesystem ----
    file << "\t\t<filesystem type='mount'>" << endl
        << "\t\t\t<source dir='" << vm->get_remote_dir() << "/lxc/rootfs"
        << "'/>" << endl;

    // ---- target mount point ----
    file << "\t\t\t<target dir='/'" << "/>" << endl;


    file << "\t\t</filesystem>" << endl;
    file << "\t<interface type='bridge'>" << endl;
    file << "\t\t<source bridge='kvmbr0'/>" << endl;
    file << "\t\t<model type='virtio'/>" <<endl;
    file << "\t</interface>" << endl;
    file << "\t<console type='pty'/>" << endl;

    attrs.clear();

    // ------------------------------------------------------------------------
    // Context Device
    // ------------------------------------------------------------------------

    if ( vm->get_template_attribute("CONTEXT",attrs) == 1 )
    {
        context = dynamic_cast<const VectorAttribute *>(attrs[0]);
        target  = context->vector_value("TARGET");
        driver  = context->vector_value("DRIVER");

        if ( !target.empty() )
        {
            file << "\t\t<disk type='file' device='cdrom'>" << endl;
            file << "\t\t\t<source file='" << vm->get_remote_dir() << "/disk."
                 << num << "'/>" << endl;
            file << "\t\t\t<target dev='" << target << "'/>" << endl;
            file << "\t\t\t<readonly/>" << endl;

            file << "\t\t\t<driver name='qemu' type='";

            if ( !driver.empty() )
            {
                file << driver << "'/>" << endl;
            }
            else
            {
                file << default_driver << "'/>" << endl;
            }

            file << "\t\t</disk>" << endl;
        }
        else
        {
            vm->log("VMM", Log::WARNING, "Could not find target device to"
                " attach context, will continue without it.");
        }
    }

    attrs.clear();

    // ------------------------------------------------------------------------
    // Network interfaces
    // ------------------------------------------------------------------------

    get_default("NIC","FILTER",default_filter);

    num = vm->get_template_attribute("NIC",attrs);

    for(int i=0; i<num; i++)
    {
        nic = dynamic_cast<const VectorAttribute *>(attrs[i]);

        if ( nic == 0 )
        {
            continue;
        }

        bridge = nic->vector_value("BRIDGE");
        mac    = nic->vector_value("MAC");
        target = nic->vector_value("TARGET");
        script = nic->vector_value("SCRIPT");
        model  = nic->vector_value("MODEL");
        ip     = nic->vector_value("IP");
        filter = nic->vector_value("FILTER");

        if ( bridge.empty() )
        {
            file << "\t\t<interface type='ethernet'>" << endl;
        }
        else
        {
            file << "\t\t<interface type='bridge'>" << endl;
            file << "\t\t\t<source bridge='" << bridge << "'/>" << endl;
        }

        if( !mac.empty() )
        {
            file << "\t\t\t<mac address='" << mac << "'/>" << endl;
        }

        if( !target.empty() )
        {
            file << "\t\t\t<target dev='" << target << "'/>" << endl;
        }

        if( !script.empty() )
        {
            file << "\t\t\t<script path='" << script << "'/>" << endl;
        }

        if( !model.empty() )
        {
            file << "\t\t\t<model type='" << model << "'/>" << endl;
        }

        if (!ip.empty() )
        {
            string * the_filter = 0;

            if (!filter.empty())
            {
                the_filter = &filter;
            }
            else if (!default_filter.empty())
            {
                the_filter = &default_filter;
            }

            if ( the_filter != 0 )
            {
                file <<"\t\t\t<filterref filter='"<< *the_filter <<"'>"<<endl;
                file << "\t\t\t\t<parameter name='IP' value='"
                     << ip << "'/>" << endl;
                file << "\t\t\t</filterref>" << endl;
            }
        }

        file << "\t\t</interface>" << endl;
    }

    attrs.clear();

    // ------------------------------------------------------------------------
    // Graphics
    // ------------------------------------------------------------------------

    if ( vm->get_template_attribute("GRAPHICS",attrs) > 0 )
    {
        graphics = dynamic_cast<const VectorAttribute *>(attrs[0]);

        if ( graphics != 0 )
        {
            type   = graphics->vector_value("TYPE");
            listen = graphics->vector_value("LISTEN");
            port   = graphics->vector_value("PORT");
            passwd = graphics->vector_value("PASSWD");
            keymap = graphics->vector_value("KEYMAP");

            if ( type == "vnc" || type == "VNC" )
            {
                file << "\t\t<graphics type='vnc'";

                if ( !listen.empty() )
                {
                    file << " listen='" << listen << "'";
                }

                if ( !port.empty() )
                {
                    file << " port='" << port << "'";
                }

                if ( !passwd.empty() )
                {
                    file << " passwd='" << passwd << "'";
                }

                if ( !keymap.empty() )
                {
                    file << " keymap='" << keymap << "'";
                }

                file << "/>" << endl;
            }
            else
            {
                vm->log("VMM", Log::WARNING,
                        "Not supported graphics type, ignored.");
            }
        }
    }

    attrs.clear();

    // ------------------------------------------------------------------------
    // Input
    // ------------------------------------------------------------------------

    if ( vm->get_template_attribute("INPUT",attrs) > 0 )
    {
        input = dynamic_cast<const VectorAttribute *>(attrs[0]);

        if ( input != 0 )
        {
            type = input->vector_value("TYPE");
            bus  = input->vector_value("BUS");

            if ( !type.empty() )
            {
                file << "\t\t<input type='" << type << "'";

                if ( !bus.empty() )
                {
                    file << " bus='" << bus << "'";
                }

                file << "/>" << endl;
            }
        }
    }

    attrs.clear();

    file << "\t</devices>" << endl;

    // ------------------------------------------------------------------------
    // Features
    // ------------------------------------------------------------------------

    num = vm->get_template_attribute("FEATURES",attrs);

    if ( num > 0 )
    {
        features = dynamic_cast<const VectorAttribute *>(attrs[0]);

        if ( features != 0 )
        {
            pae  = features->vector_value("PAE");
            acpi = features->vector_value("ACPI");
        }
    }

    if ( pae.empty() )
    {
        get_default("FEATURES", "PAE", pae);
    }

    if ( acpi.empty() )
    {
        get_default("FEATURES", "ACPI", acpi);
    }

    if( acpi == "yes" || pae == "yes" )
    {
        file << "\t<features>" << endl;

        if ( pae == "yes" )
        {
            file << "\t\t<pae/>" << endl;
        }

        if ( acpi == "yes" )
        {
            file << "\t\t<acpi/>" << endl;
        }

        file << "\t</features>" << endl;
    }

    attrs.clear();

    file << "</domain>" << endl;

    file.close();

    return 0;

error_file:
    vm->log("VMM", Log::ERROR, "Could not open LXC deployment file.");
    return -1;

error_memory:
    vm->log("VMM", Log::ERROR, "No MEMORY defined and no default provided.");
    file.close();
    return -1;
}
