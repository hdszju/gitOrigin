#ifndef CHAMPION_PHYSICAL_NODE_H
#define CHAMPION_PHYSICAL_NODE_H

#include <functional>
#include <mutex>
#include <type_traits>
#include <vector>

#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>

#include <champion_rpc.grpc.pb.h>

#include "node.h"
#include "publisher_interface.h"
#include "subscriber_interface.h"
#include "thread_pool.h"

using namespace std;

namespace champion
{
    typedef unique_ptr<champion_rpc::PhysicalNodeRPC::Stub> NodeStub;

	class Node;

	class PhysicalNode final : public champion_rpc::PhysicalNodeRPC::Service
	{
	public:
		PhysicalNode(const string & masterAddress, const string & address, const string & name);

        void addPublisher(shared_ptr<IPublisher> publisher);

        void addSubscriber(shared_ptr<ISubscriber> subscriber);

        /* void addServiceServer(shared_ptr<IServiceServer> server);

        void addServiceClient(shared_ptr<IServiceClient> client); */

        void setSubRegedCallback(string topic, function<void(string, bool)> cb);

        void setServRegedCallback(string servName, function<void(string, bool)> cb);

        shared_ptr<map<string, champion_rpc::Status>> publish(const string &topic, const string &content, const string &pub_info, int timeout_mseconds = -1);

        future<shared_ptr<map<string, champion_rpc::Status>>> asyncPublish(const string &topic, const string &content, const string &pub_info, int timeout_mseconds = -1);

        shared_ptr<champion_rpc::ServiceResponse> call(const string & serviceName, const string & request_data, const string & cli_info, int timeout_mseconds = -1);

        future<shared_ptr<champion_rpc::ServiceResponse>> asyncCall(const string & serviceName, const string & request_data, const string & cli_info, int timeout_mseconds = -1);

		grpc::Status RegisterSubscriber(grpc::ServerContext * context, const champion_rpc::SubscriberInfo * subscriberInfo,
			champion_rpc::Status * response) override;

        grpc::Status UnregisterSubscriber(grpc::ServerContext * context, const champion_rpc::SubscriberInfo * subscriberInfo,
            champion_rpc::Status * response) override;

		grpc::Status SendMessage(grpc::ServerContext * context, const champion_rpc::Message * message, champion_rpc::Status * response) override;

        grpc::Status RegisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
            champion_rpc::Status * response) override;

        grpc::Status UnregisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
            champion_rpc::Status * response) override;

        //grpc::Status InvokeService(grpc::ServerContext * context, const champion_rpc::ServiceRequest * request, champion_rpc::ServiceResponse * response) override;

        grpc::Status Ping(grpc::ServerContext * context, const champion_rpc::PingRequest * request, champion_rpc::Status * response) override;

		void spin();

		void addLogicalNode(shared_ptr<Node> node);

		void stop();

        champion_rpc::PhysicalNodeInfo getPhysicalNodeInfo();

        bool isConnectedToMaster();

        template<typename TFunction, typename... TArgs>
        auto QueueTask(TFunction&& function, TArgs&&... args) -> future<typename result_of<TFunction(TArgs...)>::type>
        {
            return _thread_pool.QueueTask(forward<TFunction>(function), forward<TArgs>(args)...);
        }

        shared_ptr<map<string, vector<string>>> checkExternalSubs(string topic = "");

        //shared_ptr<map<string, string>> checkExternalServ(string servName = "");

	private:
        ThreadPool _thread_pool;

		string _address; // todo: generate random address (hds)

		string _name; // todo: set node name (hds)

		unique_ptr<grpc::Server> _grpcServer;

		unique_ptr<champion_rpc::MasterRPC::Stub> _masterRPCStub;

		list<shared_ptr<Node>> _logicalNodes;

		map<string, list<shared_ptr<IPublisher>>> _innerPublisherLists;

		map<string, list<shared_ptr<ISubscriber>>> _innerSubscriberLists;

        map<string, shared_ptr<map<string, shared_ptr<NodeStub>>>> _outerSubscriberStubMaps; // map<topic, map<address, stub>>

        map<string, function<void(string, bool)>> _outerSubRegCallbacks;

        /* map<string, shared_ptr<IServiceServer>> _innerServiceServers;

        map<string, list<shared_ptr<IServiceClient>>> _innerServiceClientLists; */

        map<string, shared_ptr<pair<string, shared_ptr<NodeStub>>>> _outerServiceStubPairs; // map<serviceName, pair<address, stub>>

        map<string, function<void(string, bool)>> _outerServRegCallbacks;

        mutex _outerSubscriberMutex, _outerServiceMutex;

		void registerPublisherToMaster(const string & topic);

        void unregisterPublisherFromMaster(const string & topic);

        void registerSubscriberToMaster(const string & topic);

        void unregisterSubscriberFromMaster(const string & topic);

        NodeStub createNodeStub(const string & address);

        /* void registerServiceServerToMaster(shared_ptr<IServiceServer> server);

        void unregisterServiceServerFromMaster(shared_ptr<IServiceServer> server);

        void registerServiceClientToMaster(shared_ptr<IServiceClient> client);

        void unregisterServiceClientFromMaster(shared_ptr<IServiceClient> client); */

        const unsigned int _rpc_block_time_l = 10000;  // miliseconds

        const unsigned int _rpc_block_time = 1000;   // miliseconds

        future<shared_ptr<map<string, champion_rpc::Status>>> publishMessage(const string & topic, const string & content, const string & pub_info, int timeout_mseconds = -1);

        future<shared_ptr<champion_rpc::ServiceResponse>> callService(const string & serviceName, const string & request_data, const string & cli_info, int timeout_mseconds = -1);

	};
}

#endif
