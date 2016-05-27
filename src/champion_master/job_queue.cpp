#include "job_queue.h"

using namespace std;

namespace champion
{
    UpdateMessagePairJob::UpdateMessagePairJob(
        const champion_rpc::PublisherInfo & publisherInfo, 
        const champion_rpc::SubscriberInfo & subscriberInfo, 
        shared_ptr<NodeManager> nodeManager,
        shared_ptr<MessageManager> messageManager)
        : _publisherInfo(publisherInfo), _subscriberInfo(subscriberInfo), 
        _nodeManager(nodeManager), _messageManager(messageManager)
    {

    }

    JobStatus UpdateMessagePairJob::execute()
    {
        if (_callback)
        {
            auto rtn = _callback();
            if (!(rtn.flag & JobStatusFlag::Retry))
                _callback = nullptr;
            return rtn;
        }

        bool hasSub = _messageManager->hasSubscriber(_subscriberInfo);
        if (hasSub && !_nodeManager->getNode(_subscriberInfo.physical_node_info().address()))
        {
            _messageManager->removeSubscriber(_subscriberInfo);
            hasSub = false;
        }

        bool hasPub = _messageManager->hasPublisher(_publisherInfo);
        if (hasPub)
        {
            auto pub = _nodeManager->getNode(_publisherInfo.physical_node_info().address());
            if (!pub)
                _messageManager->removePublisher(_publisherInfo);
            else
            {
                auto context = std::make_shared<grpc::ClientContext>();
                context->set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
                auto cq = std::make_shared<grpc::CompletionQueue>();
                auto response = std::make_shared<champion_rpc::Status>();
                auto status = std::make_shared<grpc::Status>();

                std::shared_ptr<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>> rpc;
                if (!hasSub)
                {
                    rpc = std::make_shared<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>>(
                        pub->second->AsyncUnregisterSubscriber(context.get(), _subscriberInfo, cq.get()));
                    (*rpc.get())->Finish(response.get(), status.get(), NULL);
                }
                else
                {
                    rpc = std::make_shared<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>>(
                        pub->second->AsyncRegisterSubscriber(context.get(), _subscriberInfo, cq.get()));
                    (*rpc.get())->Finish(response.get(), status.get(), NULL);
                }
                _callback = [context, cq, response, status, rpc, hasSub](){
                    void* got_tag;
                    bool ok = false;
                    auto s = cq->AsyncNext(&got_tag, &ok, std::chrono::system_clock::now());
                    if (s == grpc::CompletionQueue::TIMEOUT)
                        return JobStatus(JobStatusFlag::Retry, "wait for last pub register/unregister finished");
                    if (!hasSub)
                        return JobStatus(JobStatusFlag::Invalid, "sub already removed");
                    else
                        return JobStatus(JobStatusFlag::Ok, "");
                };
                return this->execute();
            }
        }     
        return JobStatus(JobStatusFlag::Invalid, "pub already removed");
    }

    /* UpdateServicePairJob::UpdateServicePairJob(
        const champion_rpc::ServiceServerInfo & serverInfo,
        const champion_rpc::ServiceClientInfo & clientInfo,
        shared_ptr<NodeManager> nodeManager,
        shared_ptr<ServiceManager> serviceManager)
        : _serverInfo(serverInfo), _clientInfo(clientInfo),
        _nodeManager(nodeManager), _serviceManager(serviceManager)
    {

    }

    JobStatus UpdateServicePairJob::execute()
    {
        if (_callback)
        {
            auto rtn = _callback();
            if (!(rtn.flag & JobStatusFlag::Retry))
                _callback = nullptr;
            return rtn;
        }

        bool hasServer = _serviceManager->hasServer(_serverInfo);
        if (hasServer && !_nodeManager->getNode(_serverInfo.physical_node_info().address()))
        {
            _serviceManager->removeServer(_serverInfo);
            hasServer = false;
        }

        bool hasClient = _serviceManager->hasClient(_clientInfo);
        if (hasClient)
        {
            auto client = _nodeManager->getNode(_clientInfo.physical_node_info().address());
            if (!client)
                _serviceManager->removeClient(_clientInfo);
            else
            {
                auto context = std::make_shared<grpc::ClientContext>();
                context->set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
                auto cq = std::make_shared<grpc::CompletionQueue>();
                auto response = std::make_shared<champion_rpc::Status>();
                auto status = std::make_shared<grpc::Status>();

                std::shared_ptr<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>> rpc;
                if (!hasServer)
                {
                    rpc = std::make_shared<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>>(
                        client->second->AsyncUnregisterServiceServer(context.get(), _serverInfo, cq.get()));
                    (*rpc.get())->Finish(response.get(), status.get(), NULL);
                }
                else
                {
                    rpc = std::make_shared<std::unique_ptr<grpc::ClientAsyncResponseReader<champion_rpc::Status>>>(
                        client->second->AsyncRegisterServiceServer(context.get(), _serverInfo, cq.get()));
                    (*rpc.get())->Finish(response.get(), status.get(), NULL);
                }
                _callback = [context, cq, response, status, rpc, hasServer](){
                    void* got_tag;
                    bool ok = false;
                    auto s = cq->AsyncNext(&got_tag, &ok, std::chrono::system_clock::now());
                    if (s == grpc::CompletionQueue::TIMEOUT)
                        return JobStatus(JobStatusFlag::Retry, "wait for last client register/unregister finished");
                    if (!hasServer)
                        return JobStatus(JobStatusFlag::Invalid, "server already removed");
                    else
                        return JobStatus(JobStatusFlag::Ok, "");
                };
                return this->execute();
            }
        }
        return JobStatus(JobStatusFlag::Invalid, "client already removed");
    } */

    void JobQueue::pushFront(shared_ptr<IJob> job)
    {
        lock_guard<mutex> lock(_mutex);
        _jobDeque.push_front(job);
    }

    void JobQueue::pushBack(shared_ptr<IJob> job)
    {
        lock_guard<mutex> lock(_mutex);
        _jobDeque.push_back(job);
    }

    shared_ptr<IJob> JobQueue::popFront()
    {
        lock_guard<mutex> lock(_mutex);
        if (_jobDeque.empty())
            return NULL;
        auto result = *_jobDeque.begin();
        _jobDeque.pop_front();
        return result;
    }

    shared_ptr<IJob> JobQueue::front()
    {
        lock_guard<mutex> lock(_mutex);
        if (_jobDeque.empty())
            return NULL;
        return *_jobDeque.begin();
    }
}
