#ifndef CHAMPION_MASTER_MASTER_H
#define CHAMPION_MASTER_MASTER_H

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>

#include <champion_rpc.grpc.pb.h>

#include "message_manager.h"
//#include "service_manager.h"
#include "node_manager.h"
#include "job_queue.h"

using namespace std;

namespace champion
{
    class Master final : public champion_rpc::MasterRPC::Service
    { 
	public:
        Master(const string & address);

        void start();

        grpc::Status RegisterSubscriber(grpc::ServerContext* context, const champion_rpc::SubscriberInfo* subscriberInfo,
            champion_rpc::Status * response) override;

        grpc::Status UnregisterSubscriber(grpc::ServerContext * context, const champion_rpc::SubscriberInfo * subscriberInfo,
            champion_rpc::Status * response) override;

        grpc::Status RegisterPublisher(grpc::ServerContext* context, const champion_rpc::PublisherInfo* publisherInfo,
            champion_rpc::Status * response) override;

        grpc::Status UnregisterPublisher(grpc::ServerContext * context, const champion_rpc::PublisherInfo * publisherInfo,
            champion_rpc::Status * response) override;

      /*   grpc::Status RegisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
            champion_rpc::Status * response) override;

        grpc::Status UnregisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
            champion_rpc::Status * response) override;

        grpc::Status RegisterServiceClient(grpc::ServerContext * context, const champion_rpc::ServiceClientInfo * serviceClientInfo,
            champion_rpc::Status * response) override;

        grpc::Status UnregisterServiceClient(grpc::ServerContext * context, const champion_rpc::ServiceClientInfo * serviceClientInfo,
            champion_rpc::Status * response) override; */

        grpc::Status Ping(grpc::ServerContext * context, const champion_rpc::PingRequest * pingRequest, champion_rpc::Status * response) override;

    private:
        string _address;

		shared_ptr<MessageManager> _messageManager;

        //shared_ptr<ServiceManager> _serviceManager;

        shared_ptr<NodeManager> _nodeManager;

        JobQueue _jobQueue;

        JobQueue _dedicatedJobQueue;

        void updateMappingToNodes();

        void healthCheck();

        std::condition_variable _con_var;

	};
}

#endif