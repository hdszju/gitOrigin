#ifndef CHAMPION_MASTER_JOB_QUEUE_H
#define CHAMPION_MASTER_JOB_QUEUE_H

#include <mutex>
#include <deque>
#include <memory>
#include <chrono>
#include <champion_rpc.grpc.pb.h>
#include "node_manager.h"
#include "message_manager.h"
//#include "service_manager.h"

using namespace std;

namespace champion
{
    enum JobStatusFlag
    {
        Error   =   1 << 0,
        Invalid =   1 << 1,
        Retry   =   1 << 2,
        Ok      =   1 << 3,
    };

    inline JobStatusFlag operator|(JobStatusFlag a, JobStatusFlag b)
    {
        return static_cast<JobStatusFlag>(static_cast<int>(a) | static_cast<int>(b));
    }

    class JobStatus
    {
    public:
        JobStatus(const JobStatusFlag & flag, const string & detail)
            : flag(flag), detail(detail)
        {}

        JobStatusFlag flag;

        string detail;
    };

    class IJob
    {
    public:
        virtual JobStatus execute() = 0;

        std::chrono::steady_clock::time_point GetDeadtime() { return _deadtime; };

        void SetDeadtime(std::chrono::steady_clock::time_point deadtime) { _deadtime = deadtime; };

    protected:
        std::chrono::steady_clock::time_point _deadtime = std::chrono::steady_clock::now();

        std::function<JobStatus(void)> _callback = nullptr;
    };

    class UpdateMessagePairJob : public IJob
    {
    public:
        UpdateMessagePairJob(
            const champion_rpc::PublisherInfo & publisherInfo, 
            const champion_rpc::SubscriberInfo & subscriberInfo, 
            shared_ptr<NodeManager> nodeManager,
            shared_ptr<MessageManager> messageManager);

        JobStatus execute() override;

    private:
        champion_rpc::PublisherInfo _publisherInfo;
        
        champion_rpc::SubscriberInfo _subscriberInfo;

        shared_ptr<NodeManager> _nodeManager;

        shared_ptr<MessageManager> _messageManager;
    };

    class UpdateServicePairJob : public IJob
    {
    public:
     /*    UpdateServicePairJob(
            const champion_rpc::ServiceServerInfo & serverInfo, 
            const champion_rpc::ServiceClientInfo & clientInfo, 
            shared_ptr<NodeManager> nodeManager,
            shared_ptr<ServiceManager> serviceManager); */

        JobStatus execute() override;

    private:
        champion_rpc::ServiceServerInfo _serverInfo;

        champion_rpc::ServiceClientInfo _clientInfo;

        shared_ptr<NodeManager> _nodeManager;

        //shared_ptr<ServiceManager> _serviceManager;
    };

    class JobQueue
    {
    public:
        void pushFront(shared_ptr<IJob> job);

        void pushBack(shared_ptr<IJob> job);

        shared_ptr<IJob> popFront();

        shared_ptr<IJob> front();

    private:
        mutex _mutex;

        deque<shared_ptr<IJob>> _jobDeque;
    };
}

#endif