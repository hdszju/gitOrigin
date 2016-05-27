#include "master.h"
#include <iostream>

using namespace std;

namespace champion
{
    Master::Master(const string & address)
        :_address(address)
    {
        _nodeManager = make_shared<NodeManager>();
        _messageManager = make_shared<MessageManager>();
        //_serviceManager = make_shared<ServiceManager>();
    }

    void Master::start()
    {
        thread updateThread(&Master::updateMappingToNodes, this);
        thread healthCheckThread(&Master::healthCheck, this);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        server->Wait();

        updateThread.join();
        healthCheckThread.join();
    }

    grpc::Status Master::RegisterSubscriber(grpc::ServerContext* context, const champion_rpc::SubscriberInfo* subscriberInfo,
        champion_rpc::Status * response)
    {
        _nodeManager->addNode(subscriberInfo->physical_node_info().address());
        auto publisherList = _messageManager->addSubscriber(*subscriberInfo);
        for (auto publisher : publisherList)
        {
            _dedicatedJobQueue.pushFront(make_shared<UpdateMessagePairJob>(publisher, *subscriberInfo, _nodeManager, _messageManager));
            _con_var.notify_one();
        }
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Register Subscriber, topic:" << subscriberInfo->topic()
            << ", address:" << subscriberInfo->physical_node_info().address() << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status Master::UnregisterSubscriber(grpc::ServerContext * context, const champion_rpc::SubscriberInfo * subscriberInfo,
        champion_rpc::Status * response)
    {
        _messageManager->removeSubscriber(*subscriberInfo);
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Unregister Subscriber, topic:" << subscriberInfo->topic()
            << ", address:" << subscriberInfo->physical_node_info().address() << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status Master::RegisterPublisher(grpc::ServerContext* context, const champion_rpc::PublisherInfo* publisherInfo,
        champion_rpc::Status * response)
    {
        _nodeManager->addNode(publisherInfo->physical_node_info().address());
        auto subscriberList = _messageManager->addPublisher(*publisherInfo);
        for (auto subscriber : subscriberList)
        {
            _dedicatedJobQueue.pushFront(make_shared<UpdateMessagePairJob>(*publisherInfo, subscriber, _nodeManager, _messageManager));
            _con_var.notify_one();
        }
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Register Publisher, topic:" << publisherInfo->topic()
            << ", address:" << publisherInfo->physical_node_info().address() << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status Master::UnregisterPublisher(grpc::ServerContext * context, const champion_rpc::PublisherInfo * publisherInfo,
        champion_rpc::Status * response)
    {
        _messageManager->removePublisher(*publisherInfo);
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Unregister Publisher, topic:" << publisherInfo->topic()
            << ", address:" << publisherInfo->physical_node_info().address() << std::endl;
        return grpc::Status::OK;
    }

    /* grpc::Status Master::RegisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
        champion_rpc::Status * response)
    {
        _nodeManager->addNode(serviceServerInfo->physical_node_info().address());
        auto clientList = _serviceManager->addServer(*serviceServerInfo);
        for (auto client : clientList)
        {
            _jobQueue.pushFront(make_shared<UpdateServicePairJob>(*serviceServerInfo, client, _nodeManager, _serviceManager));
            _con_var.notify_one();
        }
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Register ServiceServer, service:" << serviceServerInfo->service_name()
            << ", address:" << serviceServerInfo->physical_node_info().address();
        return grpc::Status::OK;
    }

    grpc::Status Master::UnregisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
        champion_rpc::Status * response)
    {
        _serviceManager->removeServer(*serviceServerInfo);
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Unregister ServiceServer, service:" << serviceServerInfo->service_name()
            << ", address:" << serviceServerInfo->physical_node_info().address();
        return grpc::Status::OK;
    }

    grpc::Status Master::RegisterServiceClient(grpc::ServerContext * context, const champion_rpc::ServiceClientInfo * serviceClientInfo,
        champion_rpc::Status * response)
    {
        _nodeManager->addNode(serviceClientInfo->physical_node_info().address());
        auto server = _serviceManager->addClient(*serviceClientInfo);
        if (server.has_physical_node_info() && !server.physical_node_info().address().empty())
        {
            _jobQueue.pushFront(make_shared<UpdateServicePairJob>(server, *serviceClientInfo, _nodeManager, _serviceManager));
            _con_var.notify_one();
        }
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Register ServiceClient, service:" << serviceClientInfo->service_name()
            << ", address:" << serviceClientInfo->physical_node_info().address();
        return grpc::Status::OK;
    }

    grpc::Status Master::UnregisterServiceClient(grpc::ServerContext * context, const champion_rpc::ServiceClientInfo * serviceClientInfo,
        champion_rpc::Status * response)
    {
        _serviceManager->removeClient(*serviceClientInfo);
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Unregister ServiceClient, service:" << serviceClientInfo->service_name()
            << ", address:" << serviceClientInfo->physical_node_info().address();
        return grpc::Status::OK;
    } */

    grpc::Status Master::Ping(grpc::ServerContext * context, const champion_rpc::PingRequest * pingRequest, champion_rpc::Status * response)
    {
        response->set_code(champion_rpc::Status_Code_OK);
        return grpc::Status::OK;
    }

    void Master::updateMappingToNodes()
    {
        std::mutex mtx;
        auto dealJob = [&](std::shared_ptr<IJob> job){
            auto status = job->execute();
            if (status.flag & JobStatusFlag::Error)
            {
                std::cout << "job error: " << status.detail << std::endl; // todo : log job error (hds)
            }

            if (status.flag & JobStatusFlag::Invalid)
            {
                // std::cout << "job removed: " << status.detail; // todo : log job removed (hds)
            }
            else if (status.flag & JobStatusFlag::Retry)
            {
                _jobQueue.pushBack(job);
            }
            else if (status.flag & JobStatusFlag::Ok)
            {
                job->SetDeadtime(std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
                _dedicatedJobQueue.pushBack(job);
            }
        };
        while (true)
        {
            auto job = _jobQueue.popFront();
            if (job != NULL)
                dealJob(job);
            auto dedicatedJob = _dedicatedJobQueue.front();
            if (dedicatedJob != NULL && dedicatedJob->GetDeadtime() <= std::chrono::steady_clock::now())
            {
                _dedicatedJobQueue.popFront();
                dealJob(dedicatedJob);
            }
            if (_jobQueue.front() == NULL)
            {
                int sleep_time = 1000;
                dedicatedJob = _dedicatedJobQueue.front();
                if (dedicatedJob != NULL)
                    sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        dedicatedJob->GetDeadtime() - std::chrono::steady_clock::now()).count();
                std::unique_lock<std::mutex> lk(mtx);
                _con_var.wait_for(lk, std::chrono::milliseconds(sleep_time));
            }
        }
    }

    // healthCheck can be improved when more grpc api are available
    void Master::healthCheck()
    {
        while (true)
        {
            _nodeManager->healthCheck();
            this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}
