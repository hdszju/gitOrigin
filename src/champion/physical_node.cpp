#include "physical_node.h"
#include "error.h"
#include <iostream>

using namespace std;

namespace champion
{
    PhysicalNode::PhysicalNode(const string & masterAddress, const string & address, const string & name)
        : _masterRPCStub(champion_rpc::MasterRPC::NewStub(grpc::CreateChannel(masterAddress, grpc::InsecureCredentials())))
        , _address(address)
        , _name(name)
        , _thread_pool(8) // todo(hds): set threads size via config
    {
    }

    void PhysicalNode::addPublisher(shared_ptr<IPublisher> publisher)
    {
        auto topic = publisher->getTopic();
        auto existingPublisherList = _innerPublisherLists.find(topic);
        if (existingPublisherList == _innerPublisherLists.end())
        {
            registerPublisherToMaster(topic);
            list<shared_ptr<IPublisher>> publisherList;
            publisherList.push_back(publisher);
            _innerPublisherLists[topic] = publisherList;
        }
        else
        {
            existingPublisherList->second.push_back(publisher);
        }
    }

    void PhysicalNode::addSubscriber(shared_ptr<ISubscriber> subscriber)
    {
        auto topic = subscriber->getTopic();
        auto existingSubscriberList = _innerSubscriberLists.find(topic);
        if (existingSubscriberList == _innerSubscriberLists.end())
        {
            registerSubscriberToMaster(topic);
            list<shared_ptr<ISubscriber>> subscriberList;
            subscriberList.push_back(subscriber);
            _innerSubscriberLists[topic] = subscriberList;
        }
        else
        {
            existingSubscriberList->second.push_back(subscriber);
        }
    }

   /*  void PhysicalNode::addServiceServer(shared_ptr<IServiceServer> server)
    {
        auto serviceName = server->getServiceName();
        if (_innerServiceServers.find(serviceName) == _innerServiceServers.end())
        {
            _innerServiceServers[serviceName] = server;
            registerServiceServerToMaster(server);
        }
        else
        {
            throw invalid_operation("cannot add duplicate service server for same service name");
        }
    }

    void PhysicalNode::addServiceClient(shared_ptr<IServiceClient> client)
    {
        auto serviceName = client->getServiceName();
        auto existingClientList = _innerServiceClientLists.find(serviceName);
        if (existingClientList == _innerServiceClientLists.end())
        {
            list<shared_ptr<IServiceClient>> clientList;
            clientList.push_back(client);
            _innerServiceClientLists[serviceName] = clientList;
            registerServiceClientToMaster(client);
        }
        else
        {
            existingClientList->second.push_back(client);
        }
    } */

    void PhysicalNode::setSubRegedCallback(string topic, function<void(string, bool)> cb)
    {
        lock_guard<mutex> lock(_outerSubscriberMutex);
        _outerSubRegCallbacks[topic] = cb;
    }

    void PhysicalNode::setServRegedCallback(string servName, function<void(string, bool)> cb)
    {
        lock_guard<mutex> lock(_outerServiceMutex);
        _outerServRegCallbacks[servName] = cb;
    }

    future<shared_ptr<map<string, champion_rpc::Status>>> PhysicalNode::publishMessage(const string & topic, const string & content, const string & pub_info, int timeout_mseconds)
    {
        // todo: publish to inner subscriber will not do RPC (hds)
        shared_ptr<map<string, shared_ptr<NodeStub>>> stubs = nullptr;
        {
            lock_guard<mutex> lock(_outerSubscriberMutex);
            auto outSubscriberStubs = _outerSubscriberStubMaps.find(topic);
            if (outSubscriberStubs != _outerSubscriberStubMaps.end())
            {
                stubs = make_shared<map<string, shared_ptr<NodeStub>>>(*outSubscriberStubs->second);
            }
        }
        auto total_res = make_shared<map<string, champion_rpc::Status>>();
        auto total_cbs = make_shared<map<string, std::function<void()>>>();
        if (stubs)
        {
            for (auto s = stubs->begin(); s != stubs->end(); s++)
            {
                auto messageToSend = std::make_shared<champion_rpc::Message>();
                messageToSend->set_topic(topic);
                messageToSend->set_content(content);
                // if pub_info is "", following needn't sending to improve network performance
                messageToSend->mutable_pub_node_info()->set_publisher_name(pub_info);
                messageToSend->mutable_pub_node_info()->set_physical_node_name(_name);
                messageToSend->mutable_pub_node_info()->set_address(_address);

                auto context = std::make_shared<grpc::ClientContext>();
                if (timeout_mseconds >= 0)
                    context->set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(timeout_mseconds));
                auto cq = std::make_shared<grpc::CompletionQueue>();
                auto response = std::make_shared<champion_rpc::Status>();
                auto status = std::make_shared<grpc::Status>();
                auto add = s->first;
                auto rpc = std::make_shared<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>>(
                    (*s->second)->AsyncSendMessage(context.get(), *messageToSend, cq.get()));
         
                (*rpc)->Finish(response.get(), status.get(), (void*)(response.get()));

                auto res_cb = [messageToSend, context, cq, response, status, add, rpc, total_res](){
                    void *got_tag;
                    bool ok = false;
                    cq->Next(&got_tag, &ok);
                    if (ok && got_tag == (void*)(response.get())) {}
                    else {}
                    if (!status->ok()) {
                        response->set_code(champion_rpc::Status_Code_UNKNOWN);
                        response->set_details(string("GRPC INNER ERROR:") + status->error_message());
                    }
                    (*total_res)[add] = *response;
                };
                (*total_cbs)[add] = res_cb;
            }
        }
        return std::async(launch::deferred, [=](){
            for (auto cb = total_cbs->begin(); cb != total_cbs->end(); cb++)
            {
                cb->second();
            }
            return total_res;
        });
    }

    shared_ptr<map<string, champion_rpc::Status>> PhysicalNode::publish(const string &topic, const string &content, const string &pub_info, int timeout_mseconds)
    {
        return publishMessage(topic, content, pub_info, timeout_mseconds).get();
    }

    future<shared_ptr<map<string, champion_rpc::Status>>> PhysicalNode::asyncPublish(const string &topic, const string &content, const string &pub_info, int timeout_mseconds)
    {
        return publishMessage(topic, content, pub_info, timeout_mseconds);
    }

    future<shared_ptr<champion_rpc::ServiceResponse>> PhysicalNode::callService(
        const string & serviceName, 
        const string & request_data, 
        const string & cli_info,
        int timeout_mseconds)
    {
        auto serviceResponse = std::make_shared<champion_rpc::ServiceResponse>();
        std::function<shared_ptr<champion_rpc::ServiceResponse>()> res_cb = nullptr;
        shared_ptr<pair<string, shared_ptr<NodeStub>>> stub = nullptr;
        {
            lock_guard<mutex> lock(_outerServiceMutex);
            auto s = _outerServiceStubPairs.find(serviceName);
            if (s != _outerServiceStubPairs.end())
            {
                stub = make_shared<pair<string, shared_ptr<NodeStub>>>(*s->second);
            }
        }
        if (stub == nullptr)
        {
            serviceResponse->mutable_status()->set_code(champion_rpc::Status_Code_NOT_FOUND);
            serviceResponse->mutable_status()->set_details("no service server available");
        }
        else
        {
            auto serviceRequest = std::make_shared<champion_rpc::ServiceRequest>();            
            serviceRequest->set_service_name(serviceName);
            serviceRequest->set_request_data(request_data);
            // if cli_info is "", following needn't sending to improve network performance
            serviceRequest->mutable_cli_node_info()->set_client_name(cli_info);
            serviceRequest->mutable_cli_node_info()->set_physical_node_name(_name);
            serviceRequest->mutable_cli_node_info()->set_address(_address);

            auto context = std::make_shared<grpc::ClientContext>();
            if (timeout_mseconds >= 0)
                context->set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(timeout_mseconds));
            auto cq = std::make_shared<grpc::CompletionQueue>();
            auto status = std::make_shared<grpc::Status>();

            auto rpc = std::make_shared<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::ServiceResponse>>>(
                (*stub->second)->AsyncInvokeService(context.get(), *serviceRequest, cq.get()));

            (*rpc)->Finish(serviceResponse.get(), status.get(), (void *)serviceResponse.get());

            res_cb = [serviceRequest, serviceResponse, context, cq, status, rpc](){
                void *got_tag;
                bool ok = false;
                cq->Next(&got_tag, &ok);
                if (ok && got_tag == (void*)(serviceResponse.get())) {}
                else {}
                if (!status->ok()) {
                    serviceResponse->mutable_status()->set_code(champion_rpc::Status_Code_UNKNOWN);
                    serviceResponse->mutable_status()->set_details(string("GRPC INNER ERROR:") + status->error_message());
                }
                return serviceResponse;
            };              
        }
        return std::async(launch::deferred, [=]() -> shared_ptr<champion_rpc::ServiceResponse>{
            if (res_cb)
                return res_cb();
            else
                return serviceResponse;
        });
    }

    shared_ptr<champion_rpc::ServiceResponse> PhysicalNode::call(const string & serviceName, const string & request_data, const string & cli_info, int timeout_mseconds)
    {
        return callService(serviceName, request_data, cli_info, timeout_mseconds).get();
    }

    future<shared_ptr<champion_rpc::ServiceResponse>> PhysicalNode::asyncCall(const string & serviceName, const string & request_data, const string & cli_info, int timeout_mseconds)
    {
        return callService(serviceName, request_data, cli_info, timeout_mseconds);
    }

    shared_ptr<map<string, vector<string>>> PhysicalNode::checkExternalSubs(string topic)
    {
        auto res = make_shared<map<string, vector<string>>>();
        vector<string> topics;
        if (topic == "")
        {
            for (auto &t : _innerPublisherLists)
                topics.push_back(t.first);
        }
        else
            topics.push_back(topic);
        for (auto &t : topics)
        {
            lock_guard<mutex> l(_outerSubscriberMutex);
            auto target = _outerSubscriberStubMaps.find(t);
            if (target != _outerSubscriberStubMaps.end())
            {
                (*res)[t] = vector<string>();
                vector<string> &tmp = (*res)[t];
                for (auto &p : (*target->second))
                    tmp.push_back(p.first);
            }
        }
        return res;
    }

  /*   shared_ptr<map<string, string>> PhysicalNode::checkExternalServ(string servName)
    {
        auto res = make_shared<map<string, string>>();
        vector<string> servNames;
        if (servName == "")
        {
            for (auto &s : _innerServiceClientLists)
                servNames.push_back(s.first);
        }
        else
            servNames.push_back(servName);
        for (auto &s : servNames)
        {
            lock_guard<mutex> l(_outerServiceMutex);
            auto target = _outerServiceStubPairs.find(s);
            if (target != _outerServiceStubPairs.end())
            {
                (*res)[s] = target->first;
            }
        }
        return res;
    } */

	grpc::Status PhysicalNode::RegisterSubscriber(grpc::ServerContext* context, const champion_rpc::SubscriberInfo * subscriberInfo,
		champion_rpc::Status * response)
	{
		lock_guard<mutex> lock(_outerSubscriberMutex); // todo: need better thread safe solution for this rpc call (hds)
		auto address = subscriberInfo->physical_node_info().address();
        auto topic = subscriberInfo->topic();
		auto existingStubMap = _outerSubscriberStubMaps.find(topic);
		if (existingStubMap == _outerSubscriberStubMaps.end())
		{
			auto stubMap = make_shared<map<string, shared_ptr<NodeStub>>>();
            (*stubMap)[address] = make_shared<NodeStub>(createNodeStub(address));
			_outerSubscriberStubMaps[topic] = stubMap;
            auto cb = _outerSubRegCallbacks.find(topic);
            if (cb != _outerSubRegCallbacks.end())
                (cb->second)(topic, true);
		}
		else
		{
			auto existingStub = existingStubMap->second->find(address);
			if (existingStub == existingStubMap->second->end())
			{
                (*existingStubMap->second)[address] = make_shared<NodeStub>(createNodeStub(address));
			}
			else
			{
                response->set_code(champion_rpc::Status_Code_ALREADY_EXISTS);
                response->set_details("subscriber already exists");
				return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "");
			}
		}
        response->set_code(champion_rpc::Status_Code_OK);
        std::cout << "Register subscriber \"" << address << " " << topic << "\" from master" << std::endl;
		return grpc::Status::OK;
	}

    grpc::Status PhysicalNode::UnregisterSubscriber(grpc::ServerContext * context, const champion_rpc::SubscriberInfo * subscriberInfo,
        champion_rpc::Status * response)
    {
        lock_guard<mutex> lock(_outerSubscriberMutex);
        auto address = subscriberInfo->physical_node_info().address();
        auto topic = subscriberInfo->topic();
        auto existingStubMap = _outerSubscriberStubMaps.find(topic);
        if (existingStubMap != _outerSubscriberStubMaps.end())
        {
            auto existingStub = existingStubMap->second->find(address);
            if (existingStub != existingStubMap->second->end())
            {
                existingStubMap->second->erase(existingStub);
                if (existingStubMap->second->size() <= 0)
                {
                    _outerSubscriberStubMaps.erase(existingStubMap);
                    auto cb = _outerSubRegCallbacks.find(topic);
                    if (cb != _outerSubRegCallbacks.end())
                        (cb->second)(topic, false);
                }
                response->set_code(champion_rpc::Status_Code_OK);
                std::cout << "Unregister subscriber \"" << address << " " << topic << "\" from master" << std::endl;
                return grpc::Status::OK;
            }
        }
        response->set_code(champion_rpc::Status_Code_NOT_FOUND);
        response->set_details("subscriber not found");
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "");
    }

	grpc::Status PhysicalNode::SendMessage(grpc::ServerContext* context, const champion_rpc::Message * message, champion_rpc::Status * response)
	{
		auto existingSubscriberList = _innerSubscriberLists.find(message->topic());
		if (existingSubscriberList == _innerSubscriberLists.end() || existingSubscriberList->second.empty())
		{
            response->set_code(champion_rpc::Status_Code_FAILED_PRECONDITION);
            response->set_details("There is no subscriber for this topic");
            return grpc::Status::OK;
		}

		for (auto s = existingSubscriberList->second.begin(); s != existingSubscriberList->second.end(); s++)
		{
            if (message->has_pub_node_info())
			    (*s)->handleRawMessage(message->content(), &(message->pub_node_info()));
            else
                (*s)->handleRawMessage(message->content());
		}
        response->set_code(champion_rpc::Status_Code_OK);
		return grpc::Status::OK;
	}

    grpc::Status PhysicalNode::RegisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
        champion_rpc::Status * response)
    {
        lock_guard<mutex> lock(_outerServiceMutex); // todo: better concurrency solution (hds)
        auto serviceName = serviceServerInfo->service_name();
        auto existingStubPair = _outerServiceStubPairs.find(serviceName);
        auto address = serviceServerInfo->physical_node_info().address();
        if (existingStubPair == _outerServiceStubPairs.end() || existingStubPair->second->first != address)
        {
            auto stubPair = make_shared<pair<string, shared_ptr<NodeStub>>>(address, make_shared<NodeStub>(createNodeStub(address)));
            _outerServiceStubPairs[serviceName] = stubPair;
            auto cb = _outerServRegCallbacks.find(serviceName);
            if (cb != _outerServRegCallbacks.end())
                (cb->second)(serviceName, true);
            response->set_code(champion_rpc::Status_Code_OK);
            std::cout << "Register service server \"" << address << " " << serviceName << "\" from master" << std::endl;
            return grpc::Status::OK;
        }
        else
        {
            response->set_code(champion_rpc::Status_Code_ALREADY_EXISTS);
            response->set_details("Service server already exists");
            return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "");
        }
    }

    grpc::Status PhysicalNode::UnregisterServiceServer(grpc::ServerContext * context, const champion_rpc::ServiceServerInfo * serviceServerInfo,
        champion_rpc::Status * response)
    {
        lock_guard<mutex> lock(_outerServiceMutex);
        auto serviceName = serviceServerInfo->service_name();
        auto existingStubPair = _outerServiceStubPairs.find(serviceName);
        auto address = serviceServerInfo->physical_node_info().address();
        if (existingStubPair != _outerServiceStubPairs.end() && existingStubPair->second->first == address)
        {
            _outerServiceStubPairs.erase(existingStubPair);
            auto cb = _outerServRegCallbacks.find(serviceName);
            if (cb != _outerServRegCallbacks.end())
                (cb->second)(serviceName, false);
            response->set_code(champion_rpc::Status_Code_OK);
            std::cout << "Unregister service server \"" << address << " " << serviceName << "\" from master" << std::endl;
            return grpc::Status::OK;
        }
        response->set_code(champion_rpc::Status_Code_NOT_FOUND);
        response->set_details("subscriber not found");
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "");
    }

 /*    grpc::Status PhysicalNode::InvokeService(grpc::ServerContext * context, const champion_rpc::ServiceRequest * request, champion_rpc::ServiceResponse * response)
    {
        auto existingServer = _innerServiceServers.find(request->service_name());
        if (existingServer == _innerServiceServers.end())
        {
            response->mutable_status()->set_code(champion_rpc::Status_Code_FAILED_PRECONDITION); // todo: specific code (hds)
            response->mutable_status()->set_details("There is no server for this service");
        }
        else
            existingServer->second->invoke(request, response);
        return grpc::Status::OK;
    } */

    grpc::Status PhysicalNode::Ping(grpc::ServerContext * context, const champion_rpc::PingRequest * request, champion_rpc::Status * response)
    {
        response->set_code(champion_rpc::Status_Code_OK);
        return grpc::Status::OK;
    }

    void PhysicalNode::spin()
    {
        std::cout << "CHAMPION starts to work..." << std::endl;
        grpc::ServerBuilder builder;
        builder.AddListeningPort(_address, grpc::InsecureServerCredentials());
        builder.RegisterService(this);
        _grpcServer = move(builder.BuildAndStart());
        std::cout << "CHAMPION nodes start to work..." << std::endl;

        list<future<void>> nodeStartTasks;
        for (auto n = _logicalNodes.begin(); n != _logicalNodes.end(); n++)
            nodeStartTasks.push_back((*n)->onStart());
        for (auto t = nodeStartTasks.begin(); t != nodeStartTasks.end(); t++)
            t->wait();
        std::cout << "CHAMPION nodes are all started." << std::endl;
        _grpcServer->Wait();
        std::cout << "CHAMPION is ended." << std::endl;
    }

	void PhysicalNode::stop()
	{
        std::cout << "CHAMPION starts to end..." << std::endl;
        // stop all logical nodes
		list<future<void>> nodeStopTasks;
		for (auto n = _logicalNodes.begin(); n != _logicalNodes.end(); n++)
			nodeStopTasks.push_back((*n)->onStop());
		for (auto t = nodeStopTasks.begin(); t != nodeStopTasks.end(); t++)
			t->wait();

        // unregister service servers and clients
 /*        for (auto server : _innerServiceServers)
            unregisterServiceServerFromMaster(server.second);
        for (auto clientList : _innerServiceClientLists)
            if (!clientList.second.empty())
                unregisterServiceClientFromMaster(*clientList.second.begin()); */

        // unregister subscribers and publishers
        for (auto subscriberList : _innerSubscriberLists)
            unregisterSubscriberFromMaster(subscriberList.first);
        for (auto publisherList : _innerPublisherLists)
            unregisterPublisherFromMaster(publisherList.first);

		_grpcServer->Shutdown();
        std::cout << "CHAMPION nodes are all ended." << std::endl;        
	}

	void PhysicalNode::addLogicalNode(shared_ptr<Node> node)
	{
		_logicalNodes.push_back(node);
	}

    bool PhysicalNode::isConnectedToMaster()
    {
        grpc::ClientContext context;
        context.set_deadline(chrono::system_clock::now() + chrono::milliseconds(_rpc_block_time_l));
        champion_rpc::PingRequest request;
        champion_rpc::Status response;
        auto status = _masterRPCStub->Ping(&context, request, &response);
        return status.ok() && response.code() == champion_rpc::Status_Code_OK;
    }

	void PhysicalNode::registerPublisherToMaster(const string & topic)
	{
		grpc::ClientContext clientContext;
		champion_rpc::PublisherInfo publisherInfo;
		publisherInfo.mutable_physical_node_info()->set_address(_address);
		publisherInfo.mutable_physical_node_info()->set_name(_name);
		publisherInfo.set_topic(topic);
		champion_rpc::Status response;
        auto status = _masterRPCStub->RegisterPublisher(&clientContext, publisherInfo, &response);
		// todo : handle error (hds)
        if (status.ok() && response.code() == champion_rpc::Status_Code_OK)
            std::cout << "Register publisher to master ok: \"" << topic << "\"" << std::endl;
        else
            std::cout << "Register publisher to master fail: \"" << topic << "\"" << std::endl;
	}

    void PhysicalNode::unregisterPublisherFromMaster(const string & topic)
    {
        grpc::ClientContext clientContext;
        clientContext.set_deadline(chrono::system_clock::now() + chrono::milliseconds(_rpc_block_time));
        champion_rpc::PublisherInfo publisherInfo;
        publisherInfo.mutable_physical_node_info()->set_address(_address);
        publisherInfo.mutable_physical_node_info()->set_name(_name);
        publisherInfo.set_topic(topic);
        champion_rpc::Status result;
        auto status = _masterRPCStub->UnregisterPublisher(&clientContext, publisherInfo, &result);
        // todo : handle error (hds)
        if (status.ok() && result.code() == champion_rpc::Status_Code_OK)
            std::cout << "Unregister publisher from master ok: \"" << topic << "\"" << std::endl;
        else
            std::cout << "Unregister publisher from master fail: \"" << topic << "\"" << std::endl;
    }

	void PhysicalNode::registerSubscriberToMaster(const string & topic)
	{
		grpc::ClientContext clientContext;
		champion_rpc::SubscriberInfo subscriberInfo;
		subscriberInfo.mutable_physical_node_info()->set_address(_address);
		subscriberInfo.mutable_physical_node_info()->set_name(_name);
		subscriberInfo.set_topic(topic);
        champion_rpc::Status response;
        auto status = _masterRPCStub->RegisterSubscriber(&clientContext, subscriberInfo, &response);
		// todo: handle error (hds)
        if (status.ok() && response.code() == champion_rpc::Status_Code_OK)
            std::cout << "Register subscriber to master ok: \"" << topic << "\"" << std::endl;
        else
            std::cout << "Register subscriber to master fail: \"" << topic << "\"" << std::endl;
	}

    void PhysicalNode::unregisterSubscriberFromMaster(const string & topic)
    {
        grpc::ClientContext clientContext;
        clientContext.set_deadline(chrono::system_clock::now() + chrono::milliseconds(_rpc_block_time));
        champion_rpc::SubscriberInfo subscriberInfo;
        subscriberInfo.mutable_physical_node_info()->set_address(_address);
        subscriberInfo.mutable_physical_node_info()->set_name(_name);
        subscriberInfo.set_topic(topic);
        champion_rpc::Status result;
        auto status = _masterRPCStub->UnregisterSubscriber(&clientContext, subscriberInfo, &result);
        // todo: handle error (hds)
        if (status.ok() && result.code() == champion_rpc::Status_Code_OK)
            std::cout << "Unregister subscriber from master ok: \"" << topic << "\"" << std::endl;
        else
            std::cout << "Unregister subscriber from master fail: \"" << topic << "\"" << std::endl;
    }

   /*  void PhysicalNode::registerServiceServerToMaster(shared_ptr<IServiceServer> server)
    {
        grpc::ClientContext context;
        auto serverInfo = server->getServerInfo();
        champion_rpc::Status response;
        auto status = _masterRPCStub->RegisterServiceServer(&context, serverInfo, &response);
        // todo : handle error (hds)
        if (status.ok() && response.code() == champion_rpc::Status_Code_OK)
            std::cout << "Register Server to master ok: \"" << server->getServiceName() << "\"";
        else
            std::cout << "Register Server to master fail: \"" << server->getServiceName() << "\"";
    }

    void PhysicalNode::unregisterServiceServerFromMaster(shared_ptr<IServiceServer> server)
    {
        grpc::ClientContext context;
        context.set_deadline(chrono::system_clock::now() + chrono::milliseconds(_rpc_block_time));
        auto serverInfo = server->getServerInfo();
        champion_rpc::Status result;
        auto status = _masterRPCStub->UnregisterServiceServer(&context, serverInfo, &result);
        // todo : handle error (hds)
        if (status.ok() && result.code() == champion_rpc::Status_Code_OK)
            std::cout << "Unregister Server from master ok: \"" << server->getServiceName() << "\"";
        else
            std::cout << "Unregister Server from master fail: \"" << server->getServiceName() << "\"";
    }

    void PhysicalNode::registerServiceClientToMaster(shared_ptr<IServiceClient> client)
    {
        grpc::ClientContext context;
        auto clientInfo = client->getClientInfo();
        champion_rpc::Status response;
        auto status = _masterRPCStub->RegisterServiceClient(&context, clientInfo, &response);
        // todo : handle error (hds)
        if (status.ok() && response.code() == champion_rpc::Status_Code_OK)
            std::cout << "Register Client to master ok: \"" << client->getServiceName() << "\"";
        else
            std::cout << "Register Client to master fail: \"" << client->getServiceName() << "\"";
    }

    void PhysicalNode::unregisterServiceClientFromMaster(shared_ptr<IServiceClient> client)
    {
        grpc::ClientContext context;
        context.set_deadline(chrono::system_clock::now() + chrono::milliseconds(_rpc_block_time));
        auto clientInfo = client->getClientInfo();
        champion_rpc::Status result;
        auto status = _masterRPCStub->UnregisterServiceClient(&context, clientInfo, &result);
        // todo : handle error (hds)
        if (status.ok() && result.code() == champion_rpc::Status_Code_OK)
            std::cout << "Unregister Client from master ok: \"" << client->getServiceName() << "\"";
        else
            std::cout << "Unregister Client from master fail: \"" << client->getServiceName() << "\"";
    } */

    NodeStub PhysicalNode::createNodeStub(const string & address)
	{
		return champion_rpc::PhysicalNodeRPC::NewStub(
			grpc::CreateChannel(address, grpc::InsecureCredentials()));
	}
    
    champion_rpc::PhysicalNodeInfo PhysicalNode::getPhysicalNodeInfo()
    {
        champion_rpc::PhysicalNodeInfo info;
        info.set_address(_address);
        info.set_name(_name);
        return info;
    }
}
