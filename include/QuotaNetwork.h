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

#ifndef QUOTA_NETWORK_H_
#define QUOTA_NETWORK_H_

#include "Quota.h"

/**
 *  DataStore Quotas, defined as:
 *    NETWORK = [
 *        ID     = <ID of the datastore>
 *        LEASES = <Max. number of IPs that can be leased from net>
 *        LEASES_USED = Current number of IPs
 *    ]
 *
 *   0 = unlimited, default if missing
 */

class QuotaNetwork :  public Quota
{
public:

    QuotaNetwork(bool is_default):
        Quota("NETWORK_QUOTA",
              "NETWORK",
              NET_METRICS,
              NUM_NET_METRICS,
              is_default)
    {};

    ~QuotaNetwork(){};

    /**
     *  Check if the resource allocation will exceed the quota limits. If not
     *  the usage counters are updated
     *    @param tmpl template for the resource
     *    @param default_quotas Quotas that contain the default limits
     *    @param error string
     *    @return true if the operation can be performed
     */
    bool check(Template* tmpl, Quotas& default_quotas, string& error);

    /**
     *  Decrement usage counters when deallocating image
     *    @param tmpl template for the resource
     */
    void del(Template* tmpl);

protected:

    /**
     * Gets the default quota identified by its ID.
     *
     *    @param id of the quota
     *    @param default_quotas Quotas that contain the default limits
     *    @param va The quota, if it is found
     *
     *    @return 0 on success, -1 if not found
     */
    int get_default_quota(const string& id,
                        Quotas& default_quotas,
                        VectorAttribute **va);

    static const char * NET_METRICS[];

    static const int NUM_NET_METRICS;
};

#endif /*QUOTA_NETWORK_H_*/
