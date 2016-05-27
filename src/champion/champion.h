#ifndef CHAMPION_NROS_H
#define CHAMPION_NROS_H

#include <string>
#include <memory>
#include <list>
#include <thread>
#include <chrono>
#include <csignal>

#include "error.h"
#include "publisher.h"
#include "subscriber.h"
#include "node.h"

using namespace std;

namespace champion
{
	extern shared_ptr<PhysicalNode> physicalNode;

	void init(const string & masterAddress, const string & address, const string & name);

    //void init_log();    // should be called at the very beginning of an application

    void destroy();
	
	bool isInitialized();

    void spin();

    bool isOk();

	void addNode(shared_ptr<Node> node);

	template<class TMessage>
    shared_ptr<Publisher<TMessage>> advertise(const string &topic, const string &publisher_info = "", function<void(string, bool)> subRegcb = nullptr)
	{
        if (topic.length() <= 0)
            throw invalid_argument("topic cannot be empty");
        auto publisher = make_shared<Publisher<TMessage>>(topic, publisher_info, champion::physicalNode);
        champion::physicalNode->addPublisher(publisher);
        if (subRegcb)
            champion::physicalNode->setSubRegedCallback(topic, subRegcb);
        return publisher;
	}

	template<class TMessage>
    shared_ptr<Subscriber<TMessage>> subscribe(const string &topic, function<void(const TMessage &, const champion_rpc::PublisherNodeInfo &)> callback, bool b_req_pub_info)
	{
        if (topic.length() <= 0)
            throw invalid_argument("topic cannot be empty");
        auto subscriber = make_shared<Subscriber<TMessage>>(topic, callback, b_req_pub_info, champion::physicalNode);
        champion::physicalNode->addSubscriber(subscriber);
        return subscriber;
	}

    template<class TMessage>
    shared_ptr<Subscriber<TMessage>> subscribe(const string &topic, function<void(const TMessage &)> callback)
    {
        function<void(const TMessage &, const champion_rpc::PublisherNodeInfo &)> fmt_cb = [=](const TMessage &msg, const champion_rpc::PublisherNodeInfo &pub_info){
            callback(msg);
        };
        return champion::subscribe<TMessage>(topic, fmt_cb, false);
    }

/*     template<class TRequest, class TResponse>
    shared_ptr<ServiceServer<TRequest, TResponse>> advertiseService(
        const string & serviceName,
        function<champion_rpc::Status(const TRequest *, const champion_rpc::ClientNodeInfo &, TResponse *)> serviceFunction,
        bool b_req_cli_info)
    {
        auto server = make_shared<ServiceServer<TRequest, TResponse>>(serviceName, serviceFunction, b_req_cli_info, champion::physicalNode);
        physicalNode->addServiceServer(server);
        return server;
    }

    template<class TRequest, class TResponse>
    shared_ptr<ServiceServer<TRequest, TResponse>> advertiseService(
        const string & serviceName,
        function<champion_rpc::Status(const TRequest *, TResponse *)> serviceFunction)
    {
        function<champion_rpc::Status(const TRequest *, const champion_rpc::ClientNodeInfo &, TResponse *)> fmt_service_func =
            [=](const TRequest *req, const champion_rpc::ClientNodeInfo &cli_info, TResponse *res){
            return serviceFunction(req, res);
        };
        return champion::advertiseService(serviceName, fmt_service_func, false);
    } */

/*     template<class TRequest, class TResponse>
    shared_ptr<ServiceClient<TRequest, TResponse>> serviceClient(const string & serviceName, const string & client_info = "", function<void(string, bool)> servRegcb = nullptr)
    {
        auto client = make_shared<ServiceClient<TRequest, TResponse>>(serviceName, client_info, champion::physicalNode);
        physicalNode->addServiceClient(client);
        if (servRegcb)
            champion::physicalNode->setServRegedCallback(serviceName, servRegcb);
        return client;
    } */

}

#endif