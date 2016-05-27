#ifndef CHAMPION_MASTER_NODE_MANAGER_H
#define CHAMPION_MASTER_NODE_MANAGER_H

#include <mutex>
#include <map>
#include <champion_rpc.grpc.pb.h>

using namespace std;

namespace champion
{
    class NodeManager
    {
    public:
        void addNode(const string & nodeAddress);

        void removeNode(const string & nodeAddress);

        shared_ptr<champion_rpc::PhysicalNodeRPC::Stub> getNodeStub(const string & nodeAddress);

        shared_ptr<grpc::Channel> getNodeChannel(const string &nodeAddress);

        shared_ptr<pair<shared_ptr<grpc::Channel>, shared_ptr<champion_rpc::PhysicalNodeRPC::Stub>>> getNode(const string &nodeAddress);

        void healthCheck();

    private:
        mutex _mutex;

        map<string, shared_ptr<pair<shared_ptr<grpc::Channel>, shared_ptr<champion_rpc::PhysicalNodeRPC::Stub>>>> _nodeStubs;
    };
}

#endif