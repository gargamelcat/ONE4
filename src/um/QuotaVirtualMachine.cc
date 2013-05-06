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

#include "QuotaVirtualMachine.h"
#include "Quotas.h"

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

const char * QuotaVirtualMachine::VM_METRICS[] = {"VMS", "CPU", "MEMORY"};

const int QuotaVirtualMachine::NUM_VM_METRICS  = 3;

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int QuotaVirtualMachine::get_quota(const string& id, VectorAttribute **va)
{
    vector<Attribute*> values;
    int num;

    *va = 0;

    num = get(template_name, values);

    if ( num == 0 )
    {
        return 0;
    }

    *va = dynamic_cast<VectorAttribute *>(values[0]);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool QuotaVirtualMachine::check(Template * tmpl,
                                Quotas& default_quotas,
                                string& error)
{
    map<string, float> vm_request;

    int memory;
    float cpu;

    if ( tmpl->get("MEMORY", memory) == false  || memory <= 0 )
    {
        error = "MEMORY attribute must be a positive integer value";
        return false;
    }

    if ( tmpl->get("CPU", cpu) == false || cpu <= 0 )
    {
        error = "CPU attribute must be a positive float or integer value";
        return false;
    }

    vm_request.insert(make_pair("VMS",1));
    vm_request.insert(make_pair("MEMORY", memory));
    vm_request.insert(make_pair("CPU", cpu));

    return check_quota("", vm_request, default_quotas, error);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void QuotaVirtualMachine::del(Template * tmpl)
{
    map<string, float> vm_request;

    int memory;
    float cpu;

    if ( tmpl->get("MEMORY", memory) == false )
    {
        memory = 0;
    }

    if ( tmpl->get("CPU", cpu) == false )
    {
        cpu = 0;
    }

    vm_request.insert(make_pair("VMS",1));
    vm_request.insert(make_pair("MEMORY", memory));
    vm_request.insert(make_pair("CPU", cpu));

    del_quota("", vm_request);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int QuotaVirtualMachine::get_default_quota(
    const string& id,
    Quotas& default_quotas,
    VectorAttribute **va)
{
    return default_quotas.vm_get(id, va);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

bool QuotaVirtualMachine::update(Template * tmpl,
                                 Quotas& default_quotas,
                                 string& error)
{
    map<string, float> vm_request;

    int   delta_memory;
    float delta_cpu;

    if ( tmpl->get("MEMORY", delta_memory) == true )
    {
        vm_request.insert(make_pair("MEMORY", delta_memory));
    }

    if ( tmpl->get("CPU", delta_cpu) == true )
    {
        vm_request.insert(make_pair("CPU", delta_cpu));
    }

    return check_quota("", vm_request, default_quotas, error);
}
