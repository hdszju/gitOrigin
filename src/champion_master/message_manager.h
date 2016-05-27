#ifndef CHAMPION_MASTER_MESSAGE_MANAGER_H
#define CHAMPION_MASTER_MESSAGE_MANAGER_H

#include <map>
#include <list>
#include <mutex>
#include <memory>

#include <champion_rpc.grpc.pb.h>

using namespace std;

namespace champion
{
	class MessageManager
	{
	public:
        list<champion_rpc::SubscriberInfo> addPublisher(const champion_rpc::PublisherInfo & publisher);

        void removePublisher(const champion_rpc::PublisherInfo & publisher);

        bool hasPublisher(const champion_rpc::PublisherInfo & publisher);

		list<champion_rpc::PublisherInfo> addSubscriber(const champion_rpc::SubscriberInfo & subscriber);

        void removeSubscriber(const champion_rpc::SubscriberInfo & subscriber);

        bool hasSubscriber(const champion_rpc::SubscriberInfo & subscriber);

	private:
		mutex _mutex;

        map<string, map<string, champion_rpc::PublisherInfo>> _publisherMaps;       // map<topic, map<address, publisher>>

        map<string, map<string, champion_rpc::SubscriberInfo>> _subscriberMaps;     // map<topic, map<address, subscriber>>
	};
}

#endif