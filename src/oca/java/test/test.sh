#!/bin/bash

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

# Usage: test.sh <Test_name>
# For instance: test.sh ImageTest

JUNIT_JAR="/usr/share/java/junit4.jar"

if [ -z $ONE_LOCATION ]; then
    DB_LOCATION="/var/lib/one/one.db"
else
    DB_LOCATION="$ONE_LOCATION/var/one.db"
fi

if [ -f $DB_LOCATION ]; then
    echo "$DB_LOCATION has to be overwritten, move it to a safe place."
    exit -1
fi

echo "========================================================================="
echo "Doing $1"
echo "========================================================================="

PID=$$

oned -f &

sleep 10s;

java -cp ../lib/*:../jar/*:$JUNIT_JAR:. org.junit.runner.JUnitCore $1

CODE=$?

pkill -P $PID oned
sleep 4s;
pkill -9 -P $PID oned
rm -f $DB_LOCATION

exit $CODE