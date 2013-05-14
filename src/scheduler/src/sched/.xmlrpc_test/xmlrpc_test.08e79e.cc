#include <cstdlib>
#include <string>
#include <iostream>
#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>
#include <map>

using namespace std;

int
main(int argc, char **) {
    if (argc-1 > 0) {
        cerr << "This program has no arguments" << endl;
        exit(1);
    }
    try {
        xmlrpc_env env;
        string const serverUrl("http://localhost:8080/RPC2");
        string const methodAllocate("one.vmallocate");
        xmlrpc_c::clientSimple myClient;
        xmlrpc_c::value resultSUBMIT;

        xmlrpc_env_init(&env);

        myClient.call(serverUrl, methodAllocate, "ss",
                      &resultSUBMIT,"SESSION-GOLA&4H9109KVFSG",
                      "MEMORY=345 CPU=4 DISK=[FILE=\"img\",TYPE=cd]"
                      "DISK=[FILE=\"../f\"]");

        xmlrpc_c::value_array resultArray = xmlrpc_c::value_array(resultSUBMIT);
        vector<xmlrpc_c::value> const paramArrayValue(resultArray.vectorValueValue());

        //check posible Errors:
        xmlrpc_c::value firstvalue;
        firstvalue = static_cast<xmlrpc_c::value>(paramArrayValue[0]);
        xmlrpc_c::value_boolean status = static_cast<xmlrpc_c::value_boolean>(firstvalue);

        xmlrpc_c::value secondvalue;
        secondvalue = static_cast<xmlrpc_c::value>(paramArrayValue[1]);
        xmlrpc_c::value_string valueS = static_cast<xmlrpc_c::value_string>(secondvalue);

        if(static_cast<bool>(status)) {
            //Success, returns the id assigned to the VM:
            cout << "vmid returned: " << static_cast<string>(valueS) << endl;
            return 0;
        }
        else{ //Failure:
            string error_value=static_cast<string>(valueS);
            if (error_value.find("Error inserting",0)!=string::npos ) cout << "Error inserting VM in the database" << endl;
            else if (error_value.find("Error parsing",0)!=string::npos ) cout << "Error parsing VM template" << endl;
            else cout << "Unknown error " << static_cast<string>(valueS) << endl;
        };
    } catch (girerr::error const error) {
        cerr << "Client threw error: " << error.what() << endl;
        //"Client threw error:"
        return 20;
    } catch (std::exception const e) {
        cerr << "Client threw unexpected error." << endl;
        //Unexpected error:
        return 999;
    }
    return 0;
}
