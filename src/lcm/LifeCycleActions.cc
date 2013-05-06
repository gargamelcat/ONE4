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

#include "LifeCycleManager.h"
#include "Nebula.h"

void  LifeCycleManager::deploy_action(int vid)
{
    VirtualMachine *    vm;
    ostringstream       os;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if ( vm->get_state() == VirtualMachine::ACTIVE )
    {
        Nebula&             nd = Nebula::instance();
        TransferManager *   tm = nd.get_tm();
        time_t              thetime = time(0);
        int                 cpu,mem,disk;

        VirtualMachine::LcmState vm_state;
        TransferManager::Actions tm_action;

        //----------------------------------------------------
        //                 PROLOG STATE
        //----------------------------------------------------

        vm_state  = VirtualMachine::PROLOG;
        tm_action = TransferManager::PROLOG;

        if (vm->hasPreviousHistory())
        {
            if (vm->get_previous_action() == History::STOP_ACTION)
            {
                vm_state  = VirtualMachine::PROLOG_RESUME;
                tm_action = TransferManager::PROLOG_RESUME;
            }
            else if (vm->get_previous_action() == History::UNDEPLOY_ACTION ||
                     vm->get_previous_action() == History::UNDEPLOY_HARD_ACTION)
            {
                vm_state  = VirtualMachine::PROLOG_UNDEPLOY;
                tm_action = TransferManager::PROLOG_RESUME;
            }
        }

        vm->set_state(vm_state);

        vmpool->update(vm);

        vm->set_stime(thetime);

        vm->set_prolog_stime(thetime);

        vmpool->update_history(vm);

        vm->get_requirements(cpu,mem,disk);

        hpool->add_capacity(vm->get_hid(), vm->get_oid(), cpu, mem, disk);

        vm->log("LCM", Log::INFO, "New VM state is PROLOG.");

        //----------------------------------------------------

        tm->trigger(tm_action,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "deploy_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::suspend_action(int vid)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //                SAVE_SUSPEND STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::SAVE_SUSPEND);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->set_action(History::SUSPEND_ACTION);

        vmpool->update_history(vm);

        vm->log("LCM", Log::INFO, "New VM state is SAVE_SUSPEND");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::SAVE,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "suspend_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::stop_action(int vid)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //                SAVE_STOP STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::SAVE_STOP);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->set_action(History::STOP_ACTION);

        vmpool->update_history(vm);

        vm->log("LCM", Log::INFO, "New VM state is SAVE_STOP");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::SAVE,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "stop_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::migrate_action(int vid)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd  = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();
        int                     cpu,mem,disk;

        //----------------------------------------------------
        //                SAVE_MIGRATE STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::SAVE_MIGRATE);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->set_stime(time(0));

        vm->set_previous_action(History::MIGRATE_ACTION);

        vmpool->update_history(vm);

        vm->get_requirements(cpu,mem,disk);

        hpool->add_capacity(vm->get_hid(), vm->get_oid(), cpu, mem, disk);

        vm->log("LCM", Log::INFO, "New VM state is SAVE_MIGRATE");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::SAVE,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "migrate_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::live_migrate_action(int vid)
{
    VirtualMachine *    vm;
    ostringstream        os;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();
        int                     cpu,mem,disk;

        //----------------------------------------------------
        //                   MIGRATE STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::MIGRATE);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->set_stime(time(0));

        vm->set_previous_action(History::LIVE_MIGRATE_ACTION);

        vmpool->update_history(vm);

        vm->get_requirements(cpu,mem,disk);

        hpool->add_capacity(vm->get_hid(), vm->get_oid(), cpu, mem, disk);

        vm->log("LCM",Log::INFO,"New VM state is MIGRATE");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::MIGRATE,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "live_migrate_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::shutdown_action(int vid)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //                  SHUTDOWN STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::SHUTDOWN);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->set_action(History::SHUTDOWN_ACTION);

        vmpool->update_history(vm);

        vm->log("LCM",Log::INFO,"New VM state is SHUTDOWN");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::SHUTDOWN,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "shutdown_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::undeploy_action(int vid, bool hard)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //             SHUTDOWN_UNDEPLOY STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::SHUTDOWN_UNDEPLOY);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->log("LCM",Log::INFO,"New VM state is SHUTDOWN_UNDEPLOY");

        //----------------------------------------------------

        if (hard)
        {
            vm->set_action(History::UNDEPLOY_HARD_ACTION);

            vmm->trigger(VirtualMachineManager::CANCEL,vid);
        }
        else
        {
            vm->set_action(History::UNDEPLOY_ACTION);

            vmm->trigger(VirtualMachineManager::SHUTDOWN,vid);
        }

        vmpool->update_history(vm);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "undeploy_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::poweroff_action(int vid)
{
    poweroff_action(vid, false);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::poweroff_hard_action(int vid)
{
    poweroff_action(vid, true);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::poweroff_action(int vid, bool hard)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //             SHUTDOWN_POWEROFF STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::SHUTDOWN_POWEROFF);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->log("LCM",Log::INFO,"New VM state is SHUTDOWN_POWEROFF");

        //----------------------------------------------------

        if (hard)
        {
            vm->set_action(History::POWEROFF_HARD_ACTION);

            vmm->trigger(VirtualMachineManager::CANCEL,vid);
        }
        else
        {
            vm->set_action(History::POWEROFF_ACTION);

            vmm->trigger(VirtualMachineManager::SHUTDOWN,vid);
        }

        vmpool->update_history(vm);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "poweroff_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::restore_action(int vid)
{
    VirtualMachine *    vm;
    ostringstream       os;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state() == VirtualMachine::SUSPENDED)
    {
        Nebula&                 nd  = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();
        time_t                  the_time = time(0);

        vm->log("LCM", Log::INFO, "Restoring VM");

        //----------------------------------------------------
        //            BOOT STATE (FROM SUSPEND)
        //----------------------------------------------------
        vm->set_state(VirtualMachine::ACTIVE);

        vm->set_state(VirtualMachine::BOOT_SUSPENDED);

        vm->cp_history();

        vmpool->update(vm); //update last_seq & state

        vm->set_stime(the_time);

        vm->set_running_stime(the_time);

        vmpool->update_history(vm);

        vm->log("LCM", Log::INFO, "New state is BOOT_SUSPENDED");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::RESTORE,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "restore_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::cancel_action(int vid)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if (vm->get_state()     == VirtualMachine::ACTIVE &&
        vm->get_lcm_state() == VirtualMachine::RUNNING)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //                  CANCEL STATE
        //----------------------------------------------------

        vm->set_state(VirtualMachine::CANCEL);

        vm->set_resched(false);

        vmpool->update(vm);

        vm->set_action(History::SHUTDOWN_HARD_ACTION);

        vmpool->update_history(vm);

        vm->log("LCM", Log::INFO, "New state is CANCEL");

        //----------------------------------------------------

        vmm->trigger(VirtualMachineManager::CANCEL,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "cancel_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::restart_action(int vid)
{
    VirtualMachine *    vm;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if ((vm->get_state() == VirtualMachine::ACTIVE &&
        (vm->get_lcm_state() == VirtualMachine::UNKNOWN ||
         vm->get_lcm_state() == VirtualMachine::BOOT ||
         vm->get_lcm_state() == VirtualMachine::BOOT_UNKNOWN ||
         vm->get_lcm_state() == VirtualMachine::BOOT_POWEROFF ||
         vm->get_lcm_state() == VirtualMachine::BOOT_SUSPENDED ||
         vm->get_lcm_state() == VirtualMachine::BOOT_STOPPED ||
         vm->get_lcm_state() == VirtualMachine::BOOT_UNDEPLOY))
       ||vm->get_state() == VirtualMachine::POWEROFF)
    {
        Nebula&                 nd = Nebula::instance();
        VirtualMachineManager * vmm = nd.get_vmm();

        //----------------------------------------------------
        //       RE-START THE VM IN THE SAME HOST
        //----------------------------------------------------

        if (vm->get_state() == VirtualMachine::ACTIVE &&
            (vm->get_lcm_state() == VirtualMachine::BOOT ||
             vm->get_lcm_state() == VirtualMachine::BOOT_UNKNOWN ||
             vm->get_lcm_state() == VirtualMachine::BOOT_POWEROFF ||
             vm->get_lcm_state() == VirtualMachine::BOOT_SUSPENDED ||
             vm->get_lcm_state() == VirtualMachine::BOOT_STOPPED ||
             vm->get_lcm_state() == VirtualMachine::BOOT_UNDEPLOY))
        {
            vm->log("LCM", Log::INFO, "Sending BOOT command to VM again");
        }
        else if (vm->get_state() == VirtualMachine::ACTIVE &&
                 vm->get_lcm_state() == VirtualMachine::UNKNOWN)
        {
            vm->set_state(VirtualMachine::BOOT_UNKNOWN);

            vmpool->update(vm);

            vm->log("LCM", Log::INFO, "New VM state is BOOT_UNKNOWN");
        }
        else // if ( vm->get_state() == VirtualMachine::POWEROFF )
        {
            time_t the_time = time(0);

            vm->set_state(VirtualMachine::ACTIVE); // Only needed by poweroff
            vm->set_state(VirtualMachine::BOOT_POWEROFF);

            vm->cp_history();

            vmpool->update(vm);

            vm->set_stime(the_time);

            vm->set_running_stime(the_time);

            vmpool->update_history(vm);

            vm->log("LCM", Log::INFO, "New VM state is BOOT_POWEROFF");
        }

        vmm->trigger(VirtualMachineManager::DEPLOY,vid);
    }
    else
    {
        vm->log("LCM", Log::ERROR, "restart_action, VM in a wrong state.");
    }

    vm->unlock();

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void LifeCycleManager::delete_action(int vid)
{
    VirtualMachine * vm;

    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();
    TransferManager * tm = nd.get_tm();

    int image_id = -1;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }


    if ( vm->get_state() != VirtualMachine::ACTIVE )
    {
        vm->log("LCM", Log::ERROR, "clean_action, VM in a wrong state.");
        vm->unlock();

        return;
    }

    switch(vm->get_lcm_state())
    {
        case VirtualMachine::LCM_INIT:
            vm->log("LCM", Log::ERROR, "clean_action, VM in a wrong state.");
        break;

        case VirtualMachine::CLEANUP_RESUBMIT:
        case VirtualMachine::CLEANUP_DELETE:
            vm->set_state(VirtualMachine::CLEANUP_DELETE);
            vmpool->update(vm);

            dm->trigger(DispatchManager::DONE, vid);
        break;

        case VirtualMachine::FAILURE:
            vm->set_state(VirtualMachine::CLEANUP_DELETE);
            vmpool->update(vm);

            tm->trigger(TransferManager::EPILOG_DELETE,vid);
            dm->trigger(DispatchManager::DONE, vid);
        break;

        default:
            clean_up_vm(vm, true, image_id);
            dm->trigger(DispatchManager::DONE, vid);
        break;
    }

    vm->unlock();

    if ( image_id != -1 )
    {
        ImagePool* ipool = nd.get_ipool();
        Image*     image = ipool->get(image_id, true);

        if ( image != 0 )
        {
            image->set_state(Image::ERROR);

            ipool->update(image);

            image->unlock();
        }
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::clean_action(int vid)
{
    VirtualMachine * vm;

    Nebula&           nd = Nebula::instance();
    DispatchManager * dm = nd.get_dm();
    TransferManager * tm = nd.get_tm();

    int image_id = -1;

    vm = vmpool->get(vid,true);

    if ( vm == 0 )
    {
        return;
    }

    if ( vm->get_state() != VirtualMachine::ACTIVE )
    {
        vm->log("LCM", Log::ERROR, "clean_action, VM in a wrong state.");
        vm->unlock();

        return;
    }

    switch(vm->get_lcm_state())
    {
        case VirtualMachine::LCM_INIT:
        case VirtualMachine::CLEANUP_DELETE:
            vm->log("LCM", Log::ERROR, "clean_action, VM in a wrong state.");
        break;

        case VirtualMachine::CLEANUP_RESUBMIT:
            dm->trigger(DispatchManager::RESUBMIT, vid);
        break;

        case VirtualMachine::FAILURE:
            vm->set_state(VirtualMachine::CLEANUP_RESUBMIT);
            vmpool->update(vm);

            tm->trigger(TransferManager::EPILOG_DELETE,vid);
        break;

        default:
            clean_up_vm(vm, false, image_id);
        break;
    }

    vm->unlock();

    if ( image_id != -1 )
    {
        ImagePool* ipool = nd.get_ipool();
        Image*     image = ipool->get(image_id, true);

        if ( image != 0 )
        {
            image->set_state(Image::ERROR);

            ipool->update(image);

            image->unlock();
        }
    }
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void  LifeCycleManager::clean_up_vm(VirtualMachine * vm, bool dispose, int& image_id)
{
    int    cpu, mem, disk;
    time_t the_time = time(0);

    Nebula& nd = Nebula::instance();

    TransferManager *       tm  = nd.get_tm();
    VirtualMachineManager * vmm = nd.get_vmm();

    VirtualMachine::LcmState state = vm->get_lcm_state();
    int                      vid   = vm->get_oid();

    vm->log("LCM", Log::INFO, "New VM state is CLEANUP.");

    if (dispose)
    {
        vm->set_state(VirtualMachine::CLEANUP_DELETE);
        vm->set_action(History::DELETE_ACTION);
    }
    else
    {
        vm->set_state(VirtualMachine::CLEANUP_RESUBMIT);
        vm->set_action(History::DELETE_RECREATE_ACTION);
    }

    vm->set_resched(false);

    vm->delete_snapshots();

    vmpool->update(vm);

    vm->set_etime(the_time);
    vm->set_vm_info();
    vm->set_reason(History::USER);

    vm->get_requirements(cpu,mem,disk);
    hpool->del_capacity(vm->get_hid(), vm->get_oid(), cpu, mem, disk);

    switch (state)
    {
        case VirtualMachine::PROLOG:
        case VirtualMachine::PROLOG_RESUME:
        case VirtualMachine::PROLOG_UNDEPLOY:
            vm->set_prolog_etime(the_time);
            vmpool->update_history(vm);

            tm->trigger(TransferManager::DRIVER_CANCEL,vid);
            tm->trigger(TransferManager::EPILOG_DELETE,vid);
        break;

        case VirtualMachine::BOOT:
        case VirtualMachine::BOOT_UNKNOWN:
        case VirtualMachine::BOOT_POWEROFF:
        case VirtualMachine::BOOT_SUSPENDED:
        case VirtualMachine::BOOT_STOPPED:
        case VirtualMachine::BOOT_UNDEPLOY:
        case VirtualMachine::RUNNING:
        case VirtualMachine::UNKNOWN:
        case VirtualMachine::SHUTDOWN:
        case VirtualMachine::SHUTDOWN_POWEROFF:
        case VirtualMachine::CANCEL:
        case VirtualMachine::HOTPLUG_SNAPSHOT:
            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP,vid);
        break;

        case VirtualMachine::HOTPLUG:
            vm->clear_attach_disk();

            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP,vid);
        break;

        case VirtualMachine::HOTPLUG_NIC:
            vm->clear_attach_nic();

            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP,vid);
        break;

        case VirtualMachine::HOTPLUG_SAVEAS:
        case VirtualMachine::HOTPLUG_SAVEAS_POWEROFF:
        case VirtualMachine::HOTPLUG_SAVEAS_SUSPENDED:
            tm->trigger(TransferManager::DRIVER_CANCEL, vid);

            vm->cancel_saveas_disk(image_id);

            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP,vid);
        break;

        case VirtualMachine::MIGRATE:
            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vm->set_previous_etime(the_time);
            vm->set_previous_vm_info();
            vm->set_previous_running_etime(the_time);
            vm->set_previous_reason(History::USER);
            vmpool->update_previous_history(vm);

            hpool->del_capacity(vm->get_previous_hid(), vm->get_oid(), cpu, mem, disk);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP_BOTH,vid);
        break;

        case VirtualMachine::SAVE_STOP:
        case VirtualMachine::SAVE_SUSPEND:
            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP,vid);
        break;

        case VirtualMachine::SAVE_MIGRATE:
            vm->set_running_etime(the_time);
            vmpool->update_history(vm);

            vm->set_previous_etime(the_time);
            vm->set_previous_vm_info();
            vm->set_previous_running_etime(the_time);
            vm->set_previous_reason(History::USER);
            vmpool->update_previous_history(vm);

            hpool->del_capacity(vm->get_previous_hid(), vm->get_oid(), cpu, mem, disk);

            vmm->trigger(VirtualMachineManager::DRIVER_CANCEL,vid);
            vmm->trigger(VirtualMachineManager::CLEANUP_PREVIOUS,vid);
        break;

        case VirtualMachine::PROLOG_MIGRATE:
            vm->set_prolog_etime(the_time);
            vmpool->update_history(vm);

            tm->trigger(TransferManager::DRIVER_CANCEL,vid);
            tm->trigger(TransferManager::EPILOG_DELETE_BOTH,vid);
        break;

        case VirtualMachine::EPILOG_STOP:
        case VirtualMachine::EPILOG_UNDEPLOY:
        case VirtualMachine::EPILOG:
            vm->set_epilog_etime(the_time);
            vmpool->update_history(vm);

            tm->trigger(TransferManager::DRIVER_CANCEL,vid);
            tm->trigger(TransferManager::EPILOG_DELETE,vid);
        break;

        default: //LCM_INIT,CLEANUP_RESUBMIT, CLEANUP_DELETE, FAILURE
        break;
    }
}
