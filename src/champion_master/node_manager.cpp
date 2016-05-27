#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include "node_manager.h"
#include <iostream>

using namespace std;

namespace champion
{
    void NodeManager::addNode(const string & nodeAddress)
    {
        lock_guard<mutex> lock(_mutex);
        if (_nodeStubs.find(nodeAddress) == _nodeStubs.end())
        {
            auto channel = grpc::CreateChannel(nodeAddress, grpc::InsecureCredentials());
            auto stub = champion_rpc::PhysicalNodeRPC::NewStub(channel);
            _nodeStubs[nodeAddress] = make_shared<pair<shared_ptr<grpc::Channel>, shared_ptr<champion_rpc::PhysicalNodeRPC::Stub>>>(channel, move(stub));
        }
    }

    void NodeManager::removeNode(const string & nodeAddress)
    {
        lock_guard<mutex> lock(_mutex);
        _nodeStubs.erase(nodeAddress);
    }

    shared_ptr<champion_rpc::PhysicalNodeRPC::Stub> NodeManager::getNodeStub(const string & nodeAddress)
    {
        auto node = getNode(nodeAddress);
        return node == NULL ? NULL : node->second;
    }

    shared_ptr<grpc::Channel> NodeManager::getNodeChannel(const string &nodeAddress)
    {
        auto node = getNode(nodeAddress);
        return node == NULL ? NULL : node->first;
    }

    shared_ptr<pair<shared_ptr<grpc::Channel>, shared_ptr<champion_rpc::PhysicalNodeRPC::Stub>>> NodeManager::getNode(const string &nodeAddress)
    {
        lock_guard<mutex> lock(_mutex);
        auto stub = _nodeStubs.find(nodeAddress);
        return stub == _nodeStubs.end() ? NULL : stub->second;
    }

    void NodeManager::healthCheck()
    {
        lock_guard<mutex> lock(_mutex);
        for (auto it = _nodeStubs.begin(); it != _nodeStubs.end();)
        {
            // GetState of the node imediately, true means trying to connect the node in background
            auto status = it->second->first->GetState(true);
            if (status == GRPC_CHANNEL_FATAL_FAILURE || status == GRPC_CHANNEL_TRANSIENT_FAILURE)
            {
                std::cout << "Remove node from \""<< it->first << "\"" << std::endl;
                it = _nodeStubs.erase(it);
            }
            else
                ++it;
        }
    }
}
