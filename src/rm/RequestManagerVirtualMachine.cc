/* -------------------------------------------------------------------------- */
/* Copyright 2002-2013, OpenNebula Project (OpenNebula.org), C12G Labs        */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#include "RequestManagerVirtualMachine.h"
#include "PoolObjectAuth.h"
#include "Nebula.h"
#include "Quotas.h"

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool RequestManagerVirtualMachine::vm_authorization(
        int                     oid,
        ImageTemplate *         tmpl,
        VirtualMachineTemplate* vtmpl,
        RequestAttributes&      att,
        PoolObjectAuth *        host_perm,
        PoolObjectAuth *        ds_perm,
        AuthRequest::Operation  op)
{
    PoolObjectSQL * object;
    PoolObjectAuth vm_perms;

    object = pool->get(oid,true);

    if ( object == 0 )
    {
        failure_response(NO_EXISTS,
                get_error(object_name(auth_object),oid),
                att);

        return false;
    }

    if ( att.uid == 0 )
    {
        object->unlock();
        return true;
    }

    object->get_permissions(vm_perms);

    object->unlock();

    AuthRequest ar(att.uid, att.gid);

    ar.add_auth(op, vm_perms);

    if (host_perm != 0)
    {
        ar.add_auth(AuthRequest::MANAGE, *host_perm);
    }

    if (tmpl != 0)
    {
        string t_xml;

        ar.add_create_auth(PoolObjectSQL::IMAGE, tmpl->to_xml(t_xml));
    }

    if ( vtmpl != 0 )
    {
        VirtualMachine::set_auth_request(att.uid, ar, vtmpl);
    }

    if ( ds_perm != 0 )
    {
        ar.add_auth(AuthRequest::USE, *ds_perm);
    }

    if (UserPool::authorize(ar) == -1)
    {
        failure_response(AUTHORIZATION,
                authorization_error(ar.message, att),
                att);

        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int RequestManagerVirtualMachine::get_host_information(int hid,
                                                string& name,
                                                string& vmm,
                                                string& vnm,
                                                string& tm,
                                                string& ds_location,
                                                int&    ds_id,
                                                RequestAttributes& att,
                                                PoolObjectAuth&    host_perms)
{
    Nebula&    nd    = Nebula::instance();
    HostPool * hpool = nd.get_hpool();

    Host *      host;
    Cluster *   cluster;
    Datastore * ds;

    int cluster_id;

    host = hpool->get(hid,true);

    if ( host == 0 )
    {
        failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::HOST),hid),
                att);

        return -1;
    }

    name = host->get_name();
    vmm  = host->get_vmm_mad();
    vnm  = host->get_vnm_mad();

    host->get_permissions(host_perms);

    cluster_id = host->get_cluster_id();

    host->unlock();

    if ( cluster_id != -1 )
    {
        cluster = nd.get_clpool()->get(cluster_id, true);

        if ( cluster == 0 )
        {
            failure_response(NO_EXISTS,
                    get_error(object_name(PoolObjectSQL::CLUSTER),cluster_id),
                    att);

            return -1;
        }

        ds_id = cluster->get_ds_id();

        cluster->get_ds_location(ds_location);

        cluster->unlock();
    }
    else
    {
        ds_id = DatastorePool::SYSTEM_DS_ID;

        nd.get_configuration_attribute("DATASTORE_LOCATION", ds_location);
    }

    ds = nd.get_dspool()->get(ds_id, true);

    if ( ds == 0 )
    {
        failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::DATASTORE),ds_id),
                att);

        return -1;
    }

    if ( ds->get_type() != Datastore::SYSTEM_DS )
    {
        ostringstream oss;

        ds->unlock();

        oss << object_name(PoolObjectSQL::CLUSTER)
            << " [" << cluster_id << "] has its SYSTEM_DS set to "
            << object_name(PoolObjectSQL::DATASTORE)
            << " [" << ds_id << "], but it is not a system one.";

        failure_response(INTERNAL,
                request_error(oss.str(),""),
                att);

        return -1;
    }

    tm = ds->get_tm_mad();

    ds->unlock();

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool RequestManagerVirtualMachine::check_host(int     hid,
                                              int     cpu,
                                              int     mem,
                                              int     disk,
                                              string& error)
{
    Nebula&    nd    = Nebula::instance();
    HostPool * hpool = nd.get_hpool();

    Host * host;
    bool   test;

    host = hpool->get(hid, true);

    if (host == 0)
    {
        error = "Host no longer exists";
        return false;
    }

    test = host->test_capacity(cpu, mem, disk);

    if (!test)
    {
        ostringstream oss;

        oss << object_name(PoolObjectSQL::HOST)
            << " " << hid << " does not have enough capacity.";

        error = oss.str();
    }

    host->unlock();

    return test;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

VirtualMachine * RequestManagerVirtualMachine::get_vm(int id,
                                                      RequestAttributes& att)
{
    VirtualMachine * vm;

    vm = static_cast<VirtualMachine *>(pool->get(id,true));

    if ( vm == 0 )
    {
        failure_response(NO_EXISTS,get_error(object_name(auth_object),id), att);
        return 0;
    }

    return vm;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int RequestManagerVirtualMachine::add_history(VirtualMachine * vm,
                                       int              hid,
                                       const string&    hostname,
                                       const string&    vmm_mad,
                                       const string&    vnm_mad,
                                       const string&    tm_mad,
                                       const string&    ds_location,
                                       int              ds_id,
                                       RequestAttributes& att)
{
    string  vmdir;
    int     rc;

    VirtualMachinePool * vmpool = static_cast<VirtualMachinePool *>(pool);

    vm->add_history(hid, hostname, vmm_mad, vnm_mad, tm_mad, ds_location, ds_id);

    rc = vmpool->update_history(vm);

    if ( rc != 0 )
    {
        failure_response(INTERNAL,
                request_error("Cannot update virtual machine history",""),
                att);

        return -1;
    }

    vmpool->update(vm);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineAction::request_execute(xmlrpc_c::paramList const& paramList,
                                           RequestAttributes& att)
{
    string action_st = xmlrpc_c::value_string(paramList.getString(1));
    int    id        = xmlrpc_c::value_int(paramList.getInt(2));

    int    rc;

    Nebula& nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();

    ostringstream oss;

    AuthRequest::Operation op = auth_op;
    History::VMAction action;

    // Compatibility with 3.8
    if (action_st == "cancel")
    {
        action_st = "shutdown-hard";
    }
    else if (action_st == "restart")
    {
        action_st = "boot";
    }
    else if (action_st == "finalize")
    {
        action_st = "delete";
    }
    else if (action_st == "resubmit")
    {
        action_st = "delete-recreate";
    }
    else if (action_st == "reset")
    {
        action_st = "reboot-hard";
    }

    History::action_from_str(action_st, action);

    if (action == History::RESCHED_ACTION || action == History::UNRESCHED_ACTION)
    {
        op = AuthRequest::ADMIN;
    }

    if ( vm_authorization(id, 0, 0, att, 0, 0, op) == false )
    {
        return;
    }

    switch (action)
    {
        case History::SHUTDOWN_ACTION:
            rc = dm->shutdown(id);
            break;
        case History::HOLD_ACTION:
            rc = dm->hold(id);
            break;
        case History::RELEASE_ACTION:
            rc = dm->release(id);
            break;
        case History::STOP_ACTION:
            rc = dm->stop(id);
            break;
        case History::SHUTDOWN_HARD_ACTION:
            rc = dm->cancel(id);
            break;
        case History::SUSPEND_ACTION:
            rc = dm->suspend(id);
            break;
        case History::RESUME_ACTION:
            rc = dm->resume(id);
            break;
        case History::BOOT_ACTION:
            rc = dm->restart(id);
            break;
        case History::DELETE_ACTION:
            rc = dm->finalize(id);
            break;
        case History::DELETE_RECREATE_ACTION:
            rc = dm->resubmit(id);
            break;
        case History::REBOOT_ACTION:
            rc = dm->reboot(id);
            break;
        case History::RESCHED_ACTION:
            rc = dm->resched(id, true);
            break;
        case History::UNRESCHED_ACTION:
            rc = dm->resched(id, false);
            break;
        case History::REBOOT_HARD_ACTION:
            rc = dm->reset(id);
            break;
        case History::POWEROFF_ACTION:
            rc = dm->poweroff(id, false);
            break;
        case History::POWEROFF_HARD_ACTION:
            rc = dm->poweroff(id, true);
            break;
        case History::UNDEPLOY_ACTION:
            rc = dm->undeploy(id, false);
            break;
        case History::UNDEPLOY_HARD_ACTION:
            rc = dm->undeploy(id, true);
            break;
        default:
            rc = -3;
            break;
    }

    switch (rc)
    {
        case 0:
            success_response(id, att);
            break;
        case -1:
            failure_response(NO_EXISTS,
                    get_error(object_name(auth_object),id),
                    att);
            break;
        case -2:
            oss << "Wrong state to perform action \"" << action_st << "\"";

            failure_response(ACTION,
                    request_error(oss.str(),""),
                    att);
             break;
        case -3:
            oss << "Virtual machine action \"" << action_st
                << "\" is not supported";

            failure_response(ACTION,
                    request_error(oss.str(),""),
                    att);
            break;
        default:
            failure_response(INTERNAL,
                    request_error("Internal error","Action result not defined"),
                    att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineDeploy::request_execute(xmlrpc_c::paramList const& paramList,
                                           RequestAttributes& att)
{
    Nebula&             nd = Nebula::instance();
    DispatchManager *   dm = nd.get_dm();

    VirtualMachine * vm;
    PoolObjectAuth host_perms;

    string hostname;
    string vmm_mad;
    string vnm_mad;
    string tm_mad;
    string ds_location;
    int    ds_id;

    int id       = xmlrpc_c::value_int(paramList.getInt(1));
    int hid      = xmlrpc_c::value_int(paramList.getInt(2));
    bool enforce = false;

    bool auth = false;

    if ( paramList.size() > 3 )
    {
        enforce = xmlrpc_c::value_boolean(paramList.getBoolean(3));
    }

    if (get_host_information(hid,
                             hostname,
                             vmm_mad,
                             vnm_mad,
                             tm_mad,
                             ds_location,
                             ds_id,
                             att,
                             host_perms) != 0)
    {
        return;
    }

    auth = vm_authorization(id, 0, 0, att, &host_perms, 0, auth_op);

    if (auth == false)
    {
        return;
    }

    if ((vm = get_vm(id, att)) == 0)
    {
        return;
    }

    if (vm->get_state() != VirtualMachine::PENDING &&
        vm->get_state() != VirtualMachine::HOLD)
    {
        failure_response(ACTION,
                request_error("Wrong state to perform action",""),
                att);

        vm->unlock();
        return;
    }

    if (enforce)
    {
        int    cpu, mem, disk;
        string error;

        vm->get_requirements(cpu, mem, disk);

        vm->unlock();

        if (check_host(hid, cpu, mem, disk, error) == false)
        {
            failure_response(ACTION, request_error(error,""), att);
            return;
        }

        if ((vm = get_vm(id, att)) == 0)
        {
            return;
        }
    }

    if (add_history(vm,
                    hid,
                    hostname,
                    vmm_mad,
                    vnm_mad,
                    tm_mad,
                    ds_location,
                    ds_id,
                    att) != 0)
    {
        vm->unlock();
        return;
    }

    dm->deploy(vm);

    vm->unlock();

    success_response(id, att);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineMigrate::request_execute(xmlrpc_c::paramList const& paramList,
                                            RequestAttributes& att)
{
    Nebula&             nd = Nebula::instance();
    DispatchManager *   dm = nd.get_dm();

    VirtualMachine * vm;
    PoolObjectAuth host_perms;

    string hostname;
    string vmm_mad;
    string vnm_mad;
    string tm_mad;
    string ds_location;
    int    ds_id;

    PoolObjectAuth aux_perms;
    int     current_ds_id;
    string  aux_st;
    int     current_hid;

    int  id      = xmlrpc_c::value_int(paramList.getInt(1));
    int  hid     = xmlrpc_c::value_int(paramList.getInt(2));
    bool live    = xmlrpc_c::value_boolean(paramList.getBoolean(3));
    bool enforce = false;

    if ( paramList.size() > 4 )
    {
        enforce = xmlrpc_c::value_boolean(paramList.getBoolean(4));
    }

    bool auth = false;

    if (get_host_information(hid,
                             hostname,
                             vmm_mad,
                             vnm_mad,
                             tm_mad,
                             ds_location,
                             ds_id,
                             att,
                             host_perms) != 0)
    {
        return;
    }

    auth = vm_authorization(id, 0, 0, att, &host_perms, 0, auth_op);

    if (auth == false)
    {
        return;
    }

    if ((vm = get_vm(id, att)) == 0)
    {
        return;
    }

    if((vm->get_state()     != VirtualMachine::ACTIVE)  ||
       (vm->get_lcm_state() != VirtualMachine::RUNNING) ||
       (vm->hasPreviousHistory() && vm->get_previous_reason() == History::NONE))
    {
        failure_response(ACTION,
                request_error("Wrong state to perform action",""),
                att);

        vm->unlock();
        return;
    }

    current_hid = vm->get_hid();

    if (current_hid == hid)
    {
        ostringstream oss;

        oss << "VM is already running on "
            << object_name(PoolObjectSQL::HOST) << " [" << current_hid << "]";

        failure_response(ACTION,
                request_error(oss.str(),""),
                att);

        vm->unlock();
        return;
    }

    if (enforce)
    {
        int    cpu, mem, disk;
        string error;

        vm->get_requirements(cpu, mem, disk);

        vm->unlock();

        if (check_host(hid, cpu, mem, disk, error) == false)
        {
            failure_response(ACTION, request_error(error,""), att);
            return;
        }
    }
    else
    {
        vm->unlock();
    }

    if (get_host_information(current_hid,
                             aux_st,
                             aux_st,
                             aux_st,
                             aux_st,
                             aux_st,
                             current_ds_id,
                             att,
                             aux_perms) != 0)
    {
        return;
    }

    if ( current_ds_id != ds_id )
    {
        ostringstream oss;

        oss << "Cannot migrate to a different cluster with different system "
        << "datastore. Current " << object_name(PoolObjectSQL::HOST)
        << " [" << current_hid << "] uses system datastore [" << current_ds_id
        << "], new " << object_name(PoolObjectSQL::HOST) << " [" << hid
        << "] uses system datastore [" << ds_id << "]";

        failure_response(ACTION,
                request_error(oss.str(),""),
                att);

        return;
    }

    if ( (vm = get_vm(id, att)) == 0 )
    {
        return;
    }

    if (add_history(vm,
                    hid,
                    hostname,
                    vmm_mad,
                    vnm_mad,
                    tm_mad,
                    ds_location,
                    ds_id,
                    att) != 0)
    {
        vm->unlock();
        return;
    }

    if (live == true)
    {
        dm->live_migrate(vm);
    }
    else
    {
        dm->migrate(vm);
    }

    vm->unlock();

    success_response(id, att);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineSaveDisk::request_execute(xmlrpc_c::paramList const& paramList,
                                             RequestAttributes& att)
{
    Nebula& nd  = Nebula::instance();

    ImagePool *     ipool  = nd.get_ipool();
    DatastorePool * dspool = nd.get_dspool();
    UserPool *      upool  = nd.get_upool();

    int    id       = xmlrpc_c::value_int(paramList.getInt(1));
    int    disk_id  = xmlrpc_c::value_int(paramList.getInt(2));
    string img_name = xmlrpc_c::value_string(paramList.getString(3));
    string img_type = xmlrpc_c::value_string(paramList.getString(4));
    bool   is_hot   = false; //Optional XML-RPC argument

    if ( paramList.size() > 5 )
    {
        is_hot = xmlrpc_c::value_boolean(paramList.getBoolean(5));
    }

    VirtualMachinePool * vmpool = static_cast<VirtualMachinePool *>(pool);
    VirtualMachine * vm;
    Datastore      * ds;
    int              iid;

    string error_str;

    string driver;
    string target;
    string dev_prefix;

    // -------------------------------------------------------------------------
    // Get user's umask
    // -------------------------------------------------------------------------
    User * user = upool->get(att.uid, true);

    if ( user == 0 )
    {
        failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::USER), att.uid),
                att);

        return;
    }

    int umask = user->get_umask();

    user->unlock();

    // -------------------------------------------------------------------------
    // Prepare and check the VM/DISK to be saved_as
    // -------------------------------------------------------------------------
    if ((vm = get_vm(id, att)) == 0)
    {
        return;
    }

    if ( vm->set_saveas_state(disk_id, is_hot) != 0 )
    {
        vm->unlock();

        failure_response(INTERNAL,
                         request_error("VM has to be RUNNING, POWEROFF or"
                         " SUSPENDED to saveas disks.",""), att);
        return;
    }

    int iid_orig = vm->get_image_from_disk(disk_id, is_hot, error_str);

    if ( iid_orig == -1 )
    {
        vm->clear_saveas_state(disk_id, is_hot);

        vm->unlock();

        failure_response(INTERNAL,
                         request_error("Cannot use selected DISK", error_str),
                         att);
        return;
    }

    vmpool->update(vm);

    vm->unlock();

    // -------------------------------------------------------------------------
    // Get the data of the Image to be saved
    // -------------------------------------------------------------------------
    Image * img = ipool->get(iid_orig, true);

    if ( img == 0 )
    {
        failure_response(NO_EXISTS,
                         get_error(object_name(PoolObjectSQL::IMAGE), iid_orig),
                         att);

        if ((vm = vmpool->get(id, true)) != 0)
        {
            vm->clear_saveas_state(disk_id, is_hot);

            vmpool->update(vm);
            vm->unlock();
        }

        return;
    }

    int    ds_id   = img->get_ds_id();
    string ds_name = img->get_ds_name();
    int    size    = img->get_size();

    Image::ImageType type = img->get_type();

    img->get_template_attribute("DRIVER", driver);
    img->get_template_attribute("TARGET", target);
    img->get_template_attribute("DEV_PREFIX", dev_prefix);

    img->unlock();

    // -------------------------------------------------------------------------
    // Get the data of the DataStore for the new image
    // -------------------------------------------------------------------------
    if ((ds = dspool->get(ds_id, true)) == 0 )
    {
        failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::DATASTORE), ds_id),
                att);

        if ((vm = vmpool->get(id, true)) != 0)
        {
            vm->clear_saveas_state(disk_id, is_hot);

            vmpool->update(vm);
            vm->unlock();
        }

        return;
    }

    switch (type)
    {
        case Image::OS:
        case Image::DATABLOCK:
        case Image::CDROM:
        break;

        case Image::KERNEL:
        case Image::RAMDISK:
        case Image::CONTEXT:
            failure_response(INTERNAL,
                    request_error("Cannot save_as image of type " +
                    Image::type_to_str(type), ""), att);
        return;
    }

    string         ds_data;
    PoolObjectAuth ds_perms;

    ds->get_permissions(ds_perms);
    ds->to_xml(ds_data);

    Image::DiskType ds_disk_type = ds->get_disk_type();

    ds->unlock();

    // -------------------------------------------------------------------------
    // Create a template for the new Image
    // -------------------------------------------------------------------------
    ImageTemplate * itemplate = new ImageTemplate;
    Template        img_usage;

    itemplate->add("NAME", img_name);
    itemplate->add("SIZE", size);

    itemplate->add("SAVED_IMAGE_ID",iid_orig);
    itemplate->add("SAVED_DISK_ID",disk_id);
    itemplate->add("SAVED_VM_ID", id);

    itemplate->set_saving();

    if ( is_hot )
    {
        itemplate->set_saving_hot();
    }

    if ( img_type.empty() )
    {
        itemplate->add("TYPE", Image::type_to_str(type));
    }
    else
    {
        itemplate->add("TYPE", img_type);
    }

    if ( driver.empty() == false )
    {
        itemplate->add("DRIVER", driver);
    }

    if ( target.empty() == false )
    {
        itemplate->add("TARGET", target);
    }

    if ( dev_prefix.empty() == false )
    {
        itemplate->add("DEV_PREFIX", dev_prefix);
    }

    img_usage.add("SIZE",      size);
    img_usage.add("DATASTORE", ds_id);

    // -------------------------------------------------------------------------
    // Authorize the operation & check quotas
    // -------------------------------------------------------------------------
    bool rc_auth = vm_authorization(id, itemplate, 0, att, 0,&ds_perms,auth_op);

    if ( rc_auth == true )
    {
        rc_auth = quota_authorization(&img_usage, Quotas::DATASTORE, att);
    }

    if ( rc_auth == false)
    {
        delete itemplate;

        if ((vm = vmpool->get(id, true)) != 0)
        {
            vm->clear_saveas_state(disk_id, is_hot);

            vmpool->update(vm);
            vm->unlock();
        }

        return;
    }

    // -------------------------------------------------------------------------
    // Create the image
    // -------------------------------------------------------------------------
    int rc = ipool->allocate(att.uid,
                             att.gid,
                             att.uname,
                             att.gname,
                             umask,
                             itemplate,
                             ds_id,
                             ds_name,
                             ds_disk_type,
                             ds_data,
                             Datastore::IMAGE_DS,
                             -1,
                             &iid,
                             error_str);
    if (rc < 0)
    {
        quota_rollback(&img_usage, Quotas::DATASTORE, att);

        if ((vm = vmpool->get(id, true)) != 0)
        {
            vm->clear_saveas_state(disk_id, is_hot);

            vmpool->update(vm);
            vm->unlock();
        }

        failure_response(INTERNAL,
                allocate_error(PoolObjectSQL::IMAGE, error_str), att);
        return;
    }

    ds = dspool->get(ds_id, true);

    if ( ds != 0 )  // TODO: error otherwise or leave image in ERROR?
    {
        ds->add_image(iid);

        dspool->update(ds);

        ds->unlock();
    }

    // Return the new allocated Image ID
    success_response(iid, att);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineMonitoring::request_execute(
        xmlrpc_c::paramList const&  paramList,
        RequestAttributes&          att)
{
    int  id = xmlrpc_c::value_int(paramList.getInt(1));
    int  rc;

    ostringstream oss;

    bool auth = vm_authorization(id, 0, 0, att, 0, 0, auth_op);

    if ( auth == false )
    {
        return;
    }

    rc = (static_cast<VirtualMachinePool *>(pool))->dump_monitoring(oss, id);

    if ( rc != 0 )
    {
        failure_response(INTERNAL,request_error("Internal Error",""), att);
        return;
    }

    success_response(oss.str(), att);

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineAttach::request_execute(xmlrpc_c::paramList const& paramList,
                                            RequestAttributes& att)
{
    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();

    VirtualMachineTemplate * tmpl = new VirtualMachineTemplate();
    PoolObjectAuth           host_perms;

    int    rc;
    string error_str;

    int     id       = xmlrpc_c::value_int(paramList.getInt(1));
    string  str_tmpl = xmlrpc_c::value_string(paramList.getString(2));

    // -------------------------------------------------------------------------
    // Parse Disk template
    // -------------------------------------------------------------------------

    rc = tmpl->parse_str_or_xml(str_tmpl, error_str);

    if ( rc != 0 )
    {
        failure_response(INTERNAL, error_str, att);
        delete tmpl;

        return;
    }

    // -------------------------------------------------------------------------
    // Authorize the operation & check quotas
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, tmpl, att, 0, 0, auth_op) == false )
    {
        delete tmpl;
        return;
    }

    if ( quota_authorization(tmpl, Quotas::IMAGE, att) == false )
    {
        delete tmpl;
        return;
    }

    rc = dm->attach(id, tmpl, error_str);

    if ( rc != 0 )
    {
        quota_rollback(tmpl, Quotas::IMAGE, att);

        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(id, att);
    }

    delete tmpl;
    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineDetach::request_execute(xmlrpc_c::paramList const& paramList,
                                            RequestAttributes& att)
{
    Nebula&             nd = Nebula::instance();
    DispatchManager *   dm = nd.get_dm();

    int rc;
    string error_str;

    int     id      = xmlrpc_c::value_int(paramList.getInt(1));
    int     disk_id = xmlrpc_c::value_int(paramList.getInt(2));

    // -------------------------------------------------------------------------
    // Authorize the operation
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, 0, att, 0, 0, auth_op) == false )
    {
        return;
    }

    rc = dm->detach(id, disk_id, error_str);

    if ( rc != 0 )
    {
        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(id, att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineResize::request_execute(xmlrpc_c::paramList const& paramList,
                                           RequestAttributes& att)
{
    int     id              = xmlrpc_c::value_int(paramList.getInt(1));
    string  str_tmpl        = xmlrpc_c::value_string(paramList.getString(2));
    bool    enforce_param   = xmlrpc_c::value_boolean(paramList.getBoolean(3));

    float ncpu, ocpu, dcpu;
    int   nmemory, omemory, dmemory;
    int   nvcpu, ovcpu;

    Nebula&    nd    = Nebula::instance();
    UserPool*  upool = nd.get_upool();
    GroupPool* gpool = nd.get_gpool();
    Quotas     dquotas = nd.get_default_user_quota();
    HostPool * hpool = nd.get_hpool();

    Host * host;

    Template deltas;
    string   error_str;
    bool     rc;
    int      ret;
    int      hid = -1;

    PoolObjectAuth vm_perms;

    VirtualMachinePool * vmpool = static_cast<VirtualMachinePool *>(pool);
    VirtualMachine * vm;
    VirtualMachineTemplate tmpl;

    bool enforce = true;

    if (att.uid == UserPool::ONEADMIN_ID || att.gid == GroupPool::ONEADMIN_ID)
    {
        enforce = enforce_param;
    }

    // -------------------------------------------------------------------------
    // Parse template
    // -------------------------------------------------------------------------

    rc = tmpl.parse_str_or_xml(str_tmpl, error_str);

    if ( rc != 0 )
    {
        failure_response(INTERNAL, error_str, att);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Authorize the operation & restricted attributes                       */
    /* ---------------------------------------------------------------------- */

    if ( vm_authorization(id, 0, 0, att, 0, 0, auth_op) == false )
    {
        return;
    }

    if (att.uid != UserPool::ONEADMIN_ID && att.gid!=GroupPool::ONEADMIN_ID)
    {
        string aname;

        if (tmpl.check(aname))
        {
            ostringstream oss;

            oss << "Template includes a restricted attribute " << aname;

            failure_response(AUTHORIZATION,
                    authorization_error(oss.str(), att),
                    att);
            return;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  Get the resize values                                                 */
    /* ---------------------------------------------------------------------- */

    tmpl.get("CPU", ncpu);
    tmpl.get("VCPU", nvcpu);
    tmpl.get("MEMORY", nmemory);

    vm = vmpool->get(id, true);

    if (vm == 0)
    {
        failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::VM),id),
                att);
        return;
    }

    vm->get_permissions(vm_perms);

    vm->get_template_attribute("MEMORY", omemory);
    vm->get_template_attribute("CPU", ocpu);
    vm->get_template_attribute("VCPU", ovcpu);

    if (nmemory == 0)
    {
        nmemory = omemory;
    }

    if (ncpu == 0)
    {
        ncpu = ocpu;
    }

    if (nvcpu == 0)
    {
        nvcpu = ovcpu;
    }

    dcpu    = ncpu - ocpu;
    dmemory = nmemory - omemory;

    deltas.add("MEMORY", dmemory);
    deltas.add("CPU", dcpu);

    switch (vm->get_state())
    {
        case VirtualMachine::POWEROFF: //Only check host capacity in POWEROFF
            if (vm->hasHistory() == true)
            {
                hid = vm->get_hid();
            }
        break;

        case VirtualMachine::INIT:
        case VirtualMachine::PENDING:
        case VirtualMachine::HOLD:
        case VirtualMachine::FAILED:
        case VirtualMachine::UNDEPLOYED:
        break;

        case VirtualMachine::STOPPED:
        case VirtualMachine::DONE:
        case VirtualMachine::SUSPENDED:
        case VirtualMachine::ACTIVE:
            failure_response(ACTION,
                     request_error("Wrong state to perform action",""),
                     att);

            vm->unlock();
            return;
    }

    ret = vm->check_resize(ncpu, nmemory, nvcpu, error_str);

    vm->unlock();

    if (ret != 0)
    {
        failure_response(INTERNAL,
                request_error("Could resize the VM", error_str),
                att);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Check quotas                                                          */
    /* ---------------------------------------------------------------------- */

    if (vm_perms.uid != UserPool::ONEADMIN_ID)
    {
        User * user  = upool->get(vm_perms.uid, true);

        if ( user != 0 )
        {
            rc = user->quota.quota_update(Quotas::VM, &deltas, dquotas, error_str);

            if (rc == false)
            {
                ostringstream oss;

                oss << object_name(PoolObjectSQL::USER)
                    << " [" << vm_perms.uid << "] "
                    << error_str;

                failure_response(AUTHORIZATION,
                        request_error(oss.str(), ""),
                        att);

                user->unlock();

                return;
            }

            upool->update(user);

            user->unlock();
        }
    }

    if (vm_perms.gid != GroupPool::ONEADMIN_ID)
    {
        Group * group  = gpool->get(vm_perms.gid, true);

        if ( group != 0 )
        {
            rc = group->quota.quota_update(Quotas::VM, &deltas, dquotas, error_str);

            if (rc == false)
            {
                ostringstream oss;
                RequestAttributes att_tmp(vm_perms.uid, -1, att);

                oss << object_name(PoolObjectSQL::GROUP)
                    << " [" << vm_perms.gid << "] "
                    << error_str;

                failure_response(AUTHORIZATION,
                                 request_error(oss.str(), ""),
                                 att);

                group->unlock();

                quota_rollback(&deltas, Quotas::VM, att_tmp);

                return;
            }

            gpool->update(group);

            group->unlock();
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  Check & update host capacity                                          */
    /* ---------------------------------------------------------------------- */

    if (hid != -1)
    {
        int dcpu_host = (int) (dcpu * 100);//now in 100%
        int dmem_host = dmemory * 1024;    //now in Kilobytes

        host = hpool->get(hid, true);

        if (host == 0)
        {
            failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::HOST),hid),
                att);

            quota_rollback(&deltas, Quotas::VM, att);

            return;
        }

        if ( enforce && host->test_capacity(dcpu_host, dmem_host, 0) == false)
        {
            ostringstream oss;

            oss << object_name(PoolObjectSQL::HOST)
                << " " << hid << " does not have enough capacity.";

            failure_response(ACTION, request_error(oss.str(),""), att);

            host->unlock();

            quota_rollback(&deltas, Quotas::VM, att);

            return;
        }

        host->update_capacity(dcpu_host, dmem_host, 0);

        hpool->update(host);

        host->unlock();
    }

    /* ---------------------------------------------------------------------- */
    /*  Resize the VM                                                         */
    /* ---------------------------------------------------------------------- */

    vm = vmpool->get(id, true);

    if (vm == 0)
    {
        failure_response(NO_EXISTS,
                get_error(object_name(PoolObjectSQL::VM),id),
                att);

        quota_rollback(&deltas, Quotas::VM, att);

        if (hid != -1)
        {
            host = hpool->get(hid, true);

            if (host != 0)
            {
                host->update_capacity(-dcpu, -dmemory, 0);
                hpool->update(host);

                host->unlock();
            }
        }
        return;
    }

    //Check again state as the VM may transit to active (e.g. scheduled)
    switch (vm->get_state())
    {
        case VirtualMachine::INIT:
        case VirtualMachine::PENDING:
        case VirtualMachine::HOLD:
        case VirtualMachine::FAILED:
        case VirtualMachine::POWEROFF:
        case VirtualMachine::UNDEPLOYED:
            ret = vm->resize(ncpu, nmemory, nvcpu, error_str);

            if (ret != 0)
            {
                vm->unlock();

                failure_response(INTERNAL,
                        request_error("Could not resize the VM", error_str),
                        att);
                return;
            }

            vmpool->update(vm);
        break;

        case VirtualMachine::STOPPED:
        case VirtualMachine::DONE:
        case VirtualMachine::SUSPENDED:
        case VirtualMachine::ACTIVE:
            failure_response(ACTION,
                     request_error("Wrong state to perform action",""),
                     att);

            vm->unlock();

            quota_rollback(&deltas, Quotas::VM, att);

            if (hid != -1)
            {
                host = hpool->get(hid, true);

                if (host != 0)
                {
                    host->update_capacity(ocpu - ncpu, omemory - nmemory, 0);
                    hpool->update(host);

                    host->unlock();
                }
            }
            return;
    }

    vm->unlock();

    success_response(id, att);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineSnapshotCreate::request_execute(
        xmlrpc_c::paramList const&  paramList,
        RequestAttributes&          att)
{
    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();

    int     rc;
    int     snap_id;
    string  error_str;

    int     id   = xmlrpc_c::value_int(paramList.getInt(1));
    string  name = xmlrpc_c::value_string(paramList.getString(2));

    // -------------------------------------------------------------------------
    // Authorize the operation
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, 0, att, 0, 0, auth_op) == false )
    {
        return;
    }

    rc = dm->snapshot_create(id, name, snap_id, error_str);

    if ( rc != 0 )
    {
        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(snap_id, att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineSnapshotRevert::request_execute(
        xmlrpc_c::paramList const&  paramList,
        RequestAttributes&          att)
{
    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();

    int    rc;
    string error_str;

    int id      = xmlrpc_c::value_int(paramList.getInt(1));
    int snap_id = xmlrpc_c::value_int(paramList.getInt(2));

    // -------------------------------------------------------------------------
    // Authorize the operation
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, 0, att, 0, 0, auth_op) == false )
    {
        return;
    }

    rc = dm->snapshot_revert(id, snap_id, error_str);

    if ( rc != 0 )
    {
        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(id, att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineSnapshotDelete::request_execute(
        xmlrpc_c::paramList const&  paramList,
        RequestAttributes&          att)
{
    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();

    int    rc;
    string error_str;

    int id      = xmlrpc_c::value_int(paramList.getInt(1));
    int snap_id = xmlrpc_c::value_int(paramList.getInt(2));

    // -------------------------------------------------------------------------
    // Authorize the operation
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, 0, att, 0, 0, auth_op) == false )
    {
        return;
    }

    rc = dm->snapshot_delete(id, snap_id, error_str);

    if ( rc != 0 )
    {
        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(id, att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineAttachNic::request_execute(
        xmlrpc_c::paramList const&  paramList,
        RequestAttributes&          att)
{
    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();

    VirtualMachineTemplate tmpl;
    PoolObjectAuth           host_perms;

    int    rc;
    string error_str;

    int     id       = xmlrpc_c::value_int(paramList.getInt(1));
    string  str_tmpl = xmlrpc_c::value_string(paramList.getString(2));

    // -------------------------------------------------------------------------
    // Parse NIC template
    // -------------------------------------------------------------------------

    rc = tmpl.parse_str_or_xml(str_tmpl, error_str);

    if ( rc != 0 )
    {
        failure_response(INTERNAL, error_str, att);
        return;
    }

    // -------------------------------------------------------------------------
    // Authorize the operation, restricted attributes & check quotas
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, &tmpl, att, 0, 0, auth_op) == false )
    {
        return;
    }

    if (att.uid != UserPool::ONEADMIN_ID && att.gid!=GroupPool::ONEADMIN_ID)
    {
        string aname;

        if (tmpl.check(aname))
        {
            ostringstream oss;

            oss << "NIC includes a restricted attribute " << aname;

            failure_response(AUTHORIZATION,
                    authorization_error(oss.str(), att),
                    att);
            return;
        }
    }

    if ( quota_authorization(&tmpl, Quotas::NETWORK, att) == false )
    {
        return;
    }

    rc = dm->attach_nic(id, &tmpl, error_str);

    if ( rc != 0 )
    {
        quota_rollback(&tmpl, Quotas::NETWORK, att);

        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(id, att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void VirtualMachineDetachNic::request_execute(
        xmlrpc_c::paramList const&  paramList,
        RequestAttributes&          att)
{
    Nebula&             nd = Nebula::instance();
    DispatchManager *   dm = nd.get_dm();

    int rc;
    string error_str;

    int id      = xmlrpc_c::value_int(paramList.getInt(1));
    int nic_id  = xmlrpc_c::value_int(paramList.getInt(2));

    // -------------------------------------------------------------------------
    // Authorize the operation
    // -------------------------------------------------------------------------

    if ( vm_authorization(id, 0, 0, att, 0, 0, auth_op) == false )
    {
        return;
    }

    rc = dm->detach_nic(id, nic_id, error_str);

    if ( rc != 0 )
    {
        failure_response(ACTION,
                request_error(error_str, ""),
                att);
    }
    else
    {
        success_response(id, att);
    }

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
