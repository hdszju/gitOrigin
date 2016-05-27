#include <string>
#include <iostream>
#include "master.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;


int main(int argc, char* argv[])
{
	//cout << "origin go " << endl;
     try
    {
        //champion::set_gpr_navite_log_function("./master");
        auto address = argc > 1 ? argv[1] : "0.0.0.0:23333";
        std::cout << "Master runs on " << address << std::endl;
        champion::Master master(address);
        master.start();
    }
    catch (const std::exception &e)
    {
        std::cout << "Catch fatal exception: " << e.what() << std::endl;
        throw;
    } 
    return 0;
}
