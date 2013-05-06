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

#include "RequestManager.h"
#include "NebulaLog.h"
#include <cerrno>

#include "RequestManagerPoolInfoFilter.h"
#include "RequestManagerInfo.h"
#include "RequestManagerDelete.h"
#include "RequestManagerAllocate.h"
#include "RequestManagerUpdateTemplate.h"
#include "RequestManagerChown.h"
#include "RequestManagerChmod.h"
#include "RequestManagerClone.h"
#include "RequestManagerRename.h"

#include "RequestManagerVirtualNetwork.h"
#include "RequestManagerVirtualMachine.h"
#include "RequestManagerVMTemplate.h"
#include "RequestManagerHost.h"
#include "RequestManagerImage.h"
#include "RequestManagerUser.h"
#include "RequestManagerAcl.h"
#include "RequestManagerCluster.h"
#include "RequestManagerGroup.h"

#include "RequestManagerSystem.h"

#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <cstring>

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

extern "C" void * rm_action_loop(void *arg)
{
    RequestManager *  rm;

    if ( arg == 0 )
    {
        return 0;
    }

    NebulaLog::log("ReM",Log::INFO,"Request Manager started.");

    rm = static_cast<RequestManager *>(arg);

    rm->am.loop(0,0);

    NebulaLog::log("ReM",Log::INFO,"Request Manager stopped.");

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

extern "C" void * rm_xml_server_loop(void *arg)
{
    RequestManager *    rm;

    if ( arg == 0 )
    {
        return 0;
    }

    rm = static_cast<RequestManager *>(arg);

    // Set cancel state for the thread

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,0);

    //Start the server

    rm->AbyssServer = new xmlrpc_c::serverAbyss(xmlrpc_c::serverAbyss::constrOpt()
        .registryP(&rm->RequestManagerRegistry)
        .logFileName(rm->xml_log_file)
        .socketFd(rm->socket_fd));

    rm->AbyssServer->run();

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int RequestManager::setup_socket()
{
    int                 rc;
    int                 yes = 1;
    struct sockaddr_in  rm_addr;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if ( socket_fd == -1 )
    {
        ostringstream oss;

        oss << "Cannot open server socket: " << strerror(errno);
        NebulaLog::log("ReM",Log::ERROR,oss);

        return -1;
    }

    rc = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if ( rc == -1 )
    {
        ostringstream oss;

        oss << "Cannot set socket options: " << strerror(errno);
        NebulaLog::log("ReM",Log::ERROR,oss);

        close(socket_fd);

        return -1;
    }

    fcntl(socket_fd,F_SETFD,FD_CLOEXEC); // Close socket in MADs

    rm_addr.sin_family      = AF_INET;
    rm_addr.sin_port        = htons(port);
    rm_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(socket_fd,(struct sockaddr *) &(rm_addr),sizeof(struct sockaddr));

    if ( rc == -1)
    {
        ostringstream oss;

        oss << "Cannot bind to port " << port << " : " << strerror(errno);
        NebulaLog::log("ReM",Log::ERROR,oss);

        close(socket_fd);

        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int RequestManager::start()
{
    pthread_attr_t  pattr;
    ostringstream   oss;

    NebulaLog::log("ReM",Log::INFO,"Starting Request Manager...");

    int rc = setup_socket();

    if ( rc != 0 )
    {
        return -1;
    }

    register_xml_methods();

    pthread_attr_init (&pattr);
    pthread_attr_setdetachstate (&pattr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&rm_thread,&pattr,rm_action_loop,(void *)this);

    pthread_attr_init (&pattr);
    pthread_attr_setdetachstate (&pattr, PTHREAD_CREATE_JOINABLE);

    oss << "Starting XML-RPC server, port " << port << " ...";
    NebulaLog::log("ReM",Log::INFO,oss);

    pthread_create(&rm_xml_server_thread,&pattr,rm_xml_server_loop,(void *)this);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void RequestManager::do_action(
        const string &  action,
        void *          arg)
{
    if (action == ACTION_FINALIZE)
    {
        NebulaLog::log("ReM",Log::INFO,"Stopping Request Manager...");

        pthread_cancel(rm_xml_server_thread);

        pthread_join(rm_xml_server_thread,0);

        NebulaLog::log("ReM",Log::INFO,"XML-RPC server stopped.");

        delete AbyssServer;

        if ( socket_fd != -1 )
        {
            close(socket_fd);
        }
    }
    else
    {
        ostringstream oss;
        oss << "Unknown action name: " << action;

        NebulaLog::log("ReM", Log::ERROR, oss);
    }
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void RequestManager::register_xml_methods()
{
    // User Methods
    xmlrpc_c::methodPtr user_change_password(new UserChangePassword());
    xmlrpc_c::methodPtr user_change_auth(new UserChangeAuth());
    xmlrpc_c::methodPtr user_set_quota(new UserSetQuota());

    // Group Methods
    xmlrpc_c::methodPtr group_set_quota(new GroupSetQuota());

    // VMTemplate Methods
    xmlrpc_c::methodPtr template_instantiate(new VMTemplateInstantiate());

    // VirtualMachine Methods
    xmlrpc_c::methodPtr vm_deploy(new VirtualMachineDeploy());
    xmlrpc_c::methodPtr vm_migrate(new VirtualMachineMigrate());
    xmlrpc_c::methodPtr vm_action(new VirtualMachineAction());
    xmlrpc_c::methodPtr vm_savedisk(new VirtualMachineSaveDisk());
    xmlrpc_c::methodPtr vm_monitoring(new VirtualMachineMonitoring());
    xmlrpc_c::methodPtr vm_attach(new VirtualMachineAttach());
    xmlrpc_c::methodPtr vm_detach(new VirtualMachineDetach());
    xmlrpc_c::methodPtr vm_attachnic(new VirtualMachineAttachNic());
    xmlrpc_c::methodPtr vm_detachnic(new VirtualMachineDetachNic());
    xmlrpc_c::methodPtr vm_resize(new VirtualMachineResize());
    xmlrpc_c::methodPtr vm_snap_create(new VirtualMachineSnapshotCreate());
    xmlrpc_c::methodPtr vm_snap_revert(new VirtualMachineSnapshotRevert());
    xmlrpc_c::methodPtr vm_snap_delete(new VirtualMachineSnapshotDelete());

    xmlrpc_c::methodPtr vm_pool_acct(new VirtualMachinePoolAccounting());
    xmlrpc_c::methodPtr vm_pool_monitoring(new VirtualMachinePoolMonitoring());

    // VirtualNetwork Methods
    xmlrpc_c::methodPtr vn_addleases(new VirtualNetworkAddLeases());
    xmlrpc_c::methodPtr vn_rmleases(new VirtualNetworkRemoveLeases());
    xmlrpc_c::methodPtr vn_hold(new VirtualNetworkHold());
    xmlrpc_c::methodPtr vn_release(new VirtualNetworkRelease());

    // Update Template Methods
    xmlrpc_c::methodPtr image_update(new ImageUpdateTemplate());
    xmlrpc_c::methodPtr vm_update(new VirtualMachineUpdateTemplate());
    xmlrpc_c::methodPtr template_update(new TemplateUpdateTemplate());
    xmlrpc_c::methodPtr host_update(new HostUpdateTemplate());
    xmlrpc_c::methodPtr vn_update(new VirtualNetworkUpdateTemplate());
    xmlrpc_c::methodPtr user_update(new UserUpdateTemplate());
    xmlrpc_c::methodPtr datastore_update(new DatastoreUpdateTemplate());
    xmlrpc_c::methodPtr doc_update(new DocumentUpdateTemplate());
    xmlrpc_c::methodPtr cluster_update(new ClusterUpdateTemplate());

    // Allocate Methods
    xmlrpc_c::methodPtr vm_allocate(new VirtualMachineAllocate());
    xmlrpc_c::methodPtr image_allocate(new ImageAllocate());
    xmlrpc_c::methodPtr vn_allocate(new VirtualNetworkAllocate());
    xmlrpc_c::methodPtr group_allocate(new GroupAllocate());
    xmlrpc_c::methodPtr template_allocate(new TemplateAllocate());
    xmlrpc_c::methodPtr host_allocate(new HostAllocate());
    xmlrpc_c::methodPtr user_allocate(new  UserAllocate());
    xmlrpc_c::methodPtr datastore_allocate(new DatastoreAllocate());
    xmlrpc_c::methodPtr cluster_allocate(new ClusterAllocate());
    xmlrpc_c::methodPtr doc_allocate(new DocumentAllocate());

    // Clone Methods
    xmlrpc_c::methodPtr template_clone(new VMTemplateClone());
    xmlrpc_c::methodPtr doc_clone(new DocumentClone());

    // Delete Methods
    xmlrpc_c::methodPtr host_delete(new HostDelete());
    xmlrpc_c::methodPtr template_delete(new TemplateDelete());
    xmlrpc_c::methodPtr group_delete(new GroupDelete());
    xmlrpc_c::methodPtr vn_delete(new VirtualNetworkDelete());
    xmlrpc_c::methodPtr user_delete(new UserDelete());
    xmlrpc_c::methodPtr image_delete(new ImageDelete());
    xmlrpc_c::methodPtr datastore_delete(new DatastoreDelete());
    xmlrpc_c::methodPtr cluster_delete(new ClusterDelete());
    xmlrpc_c::methodPtr doc_delete(new DocumentDelete());

    // Info Methods
    xmlrpc_c::methodPtr vm_info(new VirtualMachineInfo());
    xmlrpc_c::methodPtr host_info(new HostInfo());
    xmlrpc_c::methodPtr template_info(new TemplateInfo());
    xmlrpc_c::methodPtr group_info(new GroupInfo());
    xmlrpc_c::methodPtr vn_info(new VirtualNetworkInfo());
    xmlrpc_c::methodPtr user_info(new UserInfo());
    xmlrpc_c::methodPtr image_info(new ImageInfo());
    xmlrpc_c::methodPtr datastore_info(new DatastoreInfo());
    xmlrpc_c::methodPtr cluster_info(new ClusterInfo());
    xmlrpc_c::methodPtr doc_info(new DocumentInfo());

    // PoolInfo Methods
    xmlrpc_c::methodPtr hostpool_info(new HostPoolInfo());
    xmlrpc_c::methodPtr grouppool_info(new GroupPoolInfo());
    xmlrpc_c::methodPtr userpool_info(new UserPoolInfo());
    xmlrpc_c::methodPtr datastorepool_info(new DatastorePoolInfo());
    xmlrpc_c::methodPtr vm_pool_info(new VirtualMachinePoolInfo());
    xmlrpc_c::methodPtr template_pool_info(new TemplatePoolInfo());
    xmlrpc_c::methodPtr vnpool_info(new VirtualNetworkPoolInfo());
    xmlrpc_c::methodPtr imagepool_info(new ImagePoolInfo());
    xmlrpc_c::methodPtr clusterpool_info(new ClusterPoolInfo());
    xmlrpc_c::methodPtr docpool_info(new DocumentPoolInfo());

    // Host Methods
    xmlrpc_c::methodPtr host_enable(new HostEnable());
    xmlrpc_c::methodPtr host_monitoring(new HostMonitoring());
    xmlrpc_c::methodPtr host_pool_monitoring(new HostPoolMonitoring());

    // Image Methods
    xmlrpc_c::methodPtr image_persistent(new ImagePersistent());
    xmlrpc_c::methodPtr image_enable(new ImageEnable());
    xmlrpc_c::methodPtr image_chtype(new ImageChangeType());
    xmlrpc_c::methodPtr image_clone(new ImageClone());

    // Chown Methods
    xmlrpc_c::methodPtr vm_chown(new VirtualMachineChown());
    xmlrpc_c::methodPtr template_chown(new TemplateChown());
    xmlrpc_c::methodPtr vn_chown(new VirtualNetworkChown());
    xmlrpc_c::methodPtr image_chown(new ImageChown());
    xmlrpc_c::methodPtr user_chown(new UserChown());
    xmlrpc_c::methodPtr datastore_chown(new DatastoreChown());
    xmlrpc_c::methodPtr doc_chown(new DocumentChown());

    // Chmod Methods
    xmlrpc_c::methodPtr vm_chmod(new VirtualMachineChmod());
    xmlrpc_c::methodPtr template_chmod(new TemplateChmod());
    xmlrpc_c::methodPtr vn_chmod(new VirtualNetworkChmod());
    xmlrpc_c::methodPtr image_chmod(new ImageChmod());
    xmlrpc_c::methodPtr datastore_chmod(new DatastoreChmod());
    xmlrpc_c::methodPtr doc_chmod(new DocumentChmod());

    // ACL Methods
    xmlrpc_c::methodPtr acl_addrule(new AclAddRule());
    xmlrpc_c::methodPtr acl_delrule(new AclDelRule());
    xmlrpc_c::methodPtr acl_info(new AclInfo());

    // Cluster Methods
    xmlrpc_c::methodPtr cluster_addhost(new ClusterAddHost());
    xmlrpc_c::methodPtr cluster_delhost(new ClusterDelHost());
    xmlrpc_c::methodPtr cluster_addds(new ClusterAddDatastore());
    xmlrpc_c::methodPtr cluster_delds(new ClusterDelDatastore());
    xmlrpc_c::methodPtr cluster_addvnet(new ClusterAddVNet());
    xmlrpc_c::methodPtr cluster_delvnet(new ClusterDelVNet());

    // System Methods
    xmlrpc_c::methodPtr system_version(new SystemVersion());
    xmlrpc_c::methodPtr system_config(new SystemConfig());

    xmlrpc_c::methodPtr user_get_default_quota(new UserQuotaInfo());
    xmlrpc_c::methodPtr user_set_default_quota(new UserQuotaUpdate());

    xmlrpc_c::methodPtr group_get_default_quota(new GroupQuotaInfo());
    xmlrpc_c::methodPtr group_set_default_quota(new GroupQuotaUpdate());

    // Rename Methods
    xmlrpc_c::methodPtr vm_rename(new VirtualMachineRename());
    xmlrpc_c::methodPtr template_rename(new TemplateRename());
    xmlrpc_c::methodPtr vn_rename(new VirtualNetworkRename());
    xmlrpc_c::methodPtr image_rename(new ImageRename());
    xmlrpc_c::methodPtr doc_rename(new DocumentRename());

    /* VM related methods  */
    RequestManagerRegistry.addMethod("one.vm.deploy", vm_deploy);
    RequestManagerRegistry.addMethod("one.vm.action", vm_action);
    RequestManagerRegistry.addMethod("one.vm.migrate", vm_migrate);
    RequestManagerRegistry.addMethod("one.vm.savedisk", vm_savedisk);
    RequestManagerRegistry.addMethod("one.vm.allocate", vm_allocate);
    RequestManagerRegistry.addMethod("one.vm.info", vm_info);
    RequestManagerRegistry.addMethod("one.vm.chown", vm_chown);
    RequestManagerRegistry.addMethod("one.vm.chmod", vm_chmod);
    RequestManagerRegistry.addMethod("one.vm.monitoring", vm_monitoring);
    RequestManagerRegistry.addMethod("one.vm.attach", vm_attach);
    RequestManagerRegistry.addMethod("one.vm.detach", vm_detach);
    RequestManagerRegistry.addMethod("one.vm.attachnic", vm_attachnic);
    RequestManagerRegistry.addMethod("one.vm.detachnic", vm_detachnic);
    RequestManagerRegistry.addMethod("one.vm.rename", vm_rename);
    RequestManagerRegistry.addMethod("one.vm.resize", vm_resize);
    RequestManagerRegistry.addMethod("one.vm.update", vm_update);
    RequestManagerRegistry.addMethod("one.vm.snapshotcreate", vm_snap_create);
    RequestManagerRegistry.addMethod("one.vm.snapshotrevert", vm_snap_revert);
    RequestManagerRegistry.addMethod("one.vm.snapshotdelete", vm_snap_delete);

    RequestManagerRegistry.addMethod("one.vmpool.info", vm_pool_info);
    RequestManagerRegistry.addMethod("one.vmpool.accounting", vm_pool_acct);
    RequestManagerRegistry.addMethod("one.vmpool.monitoring", vm_pool_monitoring);

    /* VM Template related methods*/
    RequestManagerRegistry.addMethod("one.template.update", template_update);
    RequestManagerRegistry.addMethod("one.template.instantiate",template_instantiate);
    RequestManagerRegistry.addMethod("one.template.allocate",template_allocate);
    RequestManagerRegistry.addMethod("one.template.delete", template_delete);
    RequestManagerRegistry.addMethod("one.template.info", template_info);
    RequestManagerRegistry.addMethod("one.template.chown", template_chown);
    RequestManagerRegistry.addMethod("one.template.chmod", template_chmod);
    RequestManagerRegistry.addMethod("one.template.clone", template_clone);
    RequestManagerRegistry.addMethod("one.template.rename", template_rename);

    RequestManagerRegistry.addMethod("one.templatepool.info",template_pool_info);

    /* Host related methods*/
    RequestManagerRegistry.addMethod("one.host.enable", host_enable);
    RequestManagerRegistry.addMethod("one.host.update", host_update);
    RequestManagerRegistry.addMethod("one.host.allocate", host_allocate);
    RequestManagerRegistry.addMethod("one.host.delete", host_delete);
    RequestManagerRegistry.addMethod("one.host.info", host_info);
    RequestManagerRegistry.addMethod("one.host.monitoring", host_monitoring);

    RequestManagerRegistry.addMethod("one.hostpool.info", hostpool_info);
    RequestManagerRegistry.addMethod("one.hostpool.monitoring", host_pool_monitoring);

    /* Group related methods */
    RequestManagerRegistry.addMethod("one.group.allocate",  group_allocate);
    RequestManagerRegistry.addMethod("one.group.delete",    group_delete);
    RequestManagerRegistry.addMethod("one.group.info",      group_info);
    RequestManagerRegistry.addMethod("one.group.quota",     group_set_quota);

    RequestManagerRegistry.addMethod("one.grouppool.info",  grouppool_info);

    RequestManagerRegistry.addMethod("one.groupquota.info", group_get_default_quota);
    RequestManagerRegistry.addMethod("one.groupquota.update", group_set_default_quota);

    /* Network related methods*/
    RequestManagerRegistry.addMethod("one.vn.addleases", vn_addleases);
    RequestManagerRegistry.addMethod("one.vn.rmleases", vn_rmleases);
    RequestManagerRegistry.addMethod("one.vn.hold", vn_hold);
    RequestManagerRegistry.addMethod("one.vn.release", vn_release);
    RequestManagerRegistry.addMethod("one.vn.allocate", vn_allocate);
    RequestManagerRegistry.addMethod("one.vn.update", vn_update);
    RequestManagerRegistry.addMethod("one.vn.delete", vn_delete);
    RequestManagerRegistry.addMethod("one.vn.info", vn_info);
    RequestManagerRegistry.addMethod("one.vn.chown", vn_chown);
    RequestManagerRegistry.addMethod("one.vn.chmod", vn_chmod);
    RequestManagerRegistry.addMethod("one.vn.rename", vn_rename);

    RequestManagerRegistry.addMethod("one.vnpool.info", vnpool_info);

    /* User related methods*/
    RequestManagerRegistry.addMethod("one.user.allocate", user_allocate);
    RequestManagerRegistry.addMethod("one.user.update", user_update);
    RequestManagerRegistry.addMethod("one.user.delete", user_delete);
    RequestManagerRegistry.addMethod("one.user.info", user_info);
    RequestManagerRegistry.addMethod("one.user.passwd", user_change_password);
    RequestManagerRegistry.addMethod("one.user.chgrp", user_chown);
    RequestManagerRegistry.addMethod("one.user.chauth", user_change_auth);
    RequestManagerRegistry.addMethod("one.user.quota", user_set_quota);

    RequestManagerRegistry.addMethod("one.userpool.info", userpool_info);

    RequestManagerRegistry.addMethod("one.userquota.info", user_get_default_quota);
    RequestManagerRegistry.addMethod("one.userquota.update", user_set_default_quota);

    /* Image related methods*/
    RequestManagerRegistry.addMethod("one.image.persistent", image_persistent);
    RequestManagerRegistry.addMethod("one.image.enable", image_enable);
    RequestManagerRegistry.addMethod("one.image.update", image_update);
    RequestManagerRegistry.addMethod("one.image.allocate", image_allocate);
    RequestManagerRegistry.addMethod("one.image.delete", image_delete);
    RequestManagerRegistry.addMethod("one.image.info", image_info);
    RequestManagerRegistry.addMethod("one.image.chown", image_chown);
    RequestManagerRegistry.addMethod("one.image.chmod", image_chmod);
    RequestManagerRegistry.addMethod("one.image.chtype", image_chtype);
    RequestManagerRegistry.addMethod("one.image.clone", image_clone);
    RequestManagerRegistry.addMethod("one.image.rename", image_rename);

    RequestManagerRegistry.addMethod("one.imagepool.info", imagepool_info);

    /* ACL related methods */
    RequestManagerRegistry.addMethod("one.acl.addrule", acl_addrule);
    RequestManagerRegistry.addMethod("one.acl.delrule", acl_delrule);
    RequestManagerRegistry.addMethod("one.acl.info",    acl_info);

    /* Datastore related methods */
    RequestManagerRegistry.addMethod("one.datastore.allocate",datastore_allocate);
    RequestManagerRegistry.addMethod("one.datastore.delete",  datastore_delete);
    RequestManagerRegistry.addMethod("one.datastore.info",    datastore_info);
    RequestManagerRegistry.addMethod("one.datastore.update",  datastore_update);
    RequestManagerRegistry.addMethod("one.datastore.chown",   datastore_chown);
    RequestManagerRegistry.addMethod("one.datastore.chmod",   datastore_chmod);

    RequestManagerRegistry.addMethod("one.datastorepool.info",datastorepool_info);

    /* Cluster related methods */
    RequestManagerRegistry.addMethod("one.cluster.allocate",cluster_allocate);
    RequestManagerRegistry.addMethod("one.cluster.delete",  cluster_delete);
    RequestManagerRegistry.addMethod("one.cluster.info",    cluster_info);
    RequestManagerRegistry.addMethod("one.cluster.update",  cluster_update);

    RequestManagerRegistry.addMethod("one.cluster.addhost", cluster_addhost);
    RequestManagerRegistry.addMethod("one.cluster.delhost", cluster_delhost);
    RequestManagerRegistry.addMethod("one.cluster.adddatastore", cluster_addds);
    RequestManagerRegistry.addMethod("one.cluster.deldatastore", cluster_delds);
    RequestManagerRegistry.addMethod("one.cluster.addvnet", cluster_addvnet);
    RequestManagerRegistry.addMethod("one.cluster.delvnet", cluster_delvnet);

    RequestManagerRegistry.addMethod("one.clusterpool.info",clusterpool_info);

    /* Generic Document objects related methods*/
    RequestManagerRegistry.addMethod("one.document.allocate",doc_allocate);
    RequestManagerRegistry.addMethod("one.document.delete",  doc_delete);
    RequestManagerRegistry.addMethod("one.document.info",    doc_info);
    RequestManagerRegistry.addMethod("one.document.update",  doc_update);
    RequestManagerRegistry.addMethod("one.document.chown",   doc_chown);
    RequestManagerRegistry.addMethod("one.document.chmod",   doc_chmod);
    RequestManagerRegistry.addMethod("one.document.clone",   doc_clone);
    RequestManagerRegistry.addMethod("one.document.rename",   doc_rename);

    RequestManagerRegistry.addMethod("one.documentpool.info",docpool_info);

    /* System related methods */
    RequestManagerRegistry.addMethod("one.system.version", system_version);
    RequestManagerRegistry.addMethod("one.system.config", system_config);
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

