#include "champion.h"
//#include "log.h"

using namespace std;

namespace champion
{
	shared_ptr<PhysicalNode> physicalNode = NULL;

	void breakSignalHandler(int sig)
	{
		physicalNode->stop();
	}

	void init(const string & masterAddress, const string & address, const string & name)
	{
        if (isInitialized())
            throw initialize_again("Cannot initialize champion multiple times.");
		physicalNode = make_shared<PhysicalNode>(masterAddress, address, name);
        if (!physicalNode->isConnectedToMaster())
            throw initialize_error("Cannot connect to Master");
		signal(SIGINT, &champion::breakSignalHandler);
		signal(SIGABRT, &champion::breakSignalHandler);
		signal(SIGTERM, &champion::breakSignalHandler);
	}

 /*    void init_log()
    {
        //set_gpr_navite_log_function();
    } */

    void destroy()
    {
        physicalNode = NULL;
    }

	bool isInitialized()
	{
		return physicalNode != NULL;
	}

    void spin()
	{
		physicalNode->spin();
    }

	void addNode(shared_ptr<Node> node)
	{
		physicalNode->addLogicalNode(node);
	}
}
