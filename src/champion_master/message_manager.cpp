#include "message_manager.h"
#include <algorithm>
using namespace std;

namespace champion
{
	list<champion_rpc::SubscriberInfo> MessageManager::addPublisher(const champion_rpc::PublisherInfo & publisher)
	{
		lock_guard<mutex> lock(_mutex);
        auto existingPublisherMap = _publisherMaps.find(publisher.topic());
        if (existingPublisherMap == _publisherMaps.end())
        {
            map<string, champion_rpc::PublisherInfo> publisherMap;
            publisherMap[publisher.physical_node_info().address()] = publisher;
            _publisherMaps[publisher.topic()] = publisherMap;
        }
        else
        {
            existingPublisherMap->second[publisher.physical_node_info().address()] = publisher;
        }
        list<champion_rpc::SubscriberInfo> subscriberList;
        auto subscriberMap = _subscriberMaps.find(publisher.topic());
        if (subscriberMap != _subscriberMaps.end())
        {
            transform(subscriberMap->second.begin(), subscriberMap->second.end(), back_inserter(subscriberList),
                [](map<string, champion_rpc::SubscriberInfo>::value_type & value){return value.second; });
        }
        return subscriberList;
	}

    void MessageManager::removePublisher(const champion_rpc::PublisherInfo & publisher)
    {
        lock_guard<mutex> lock(_mutex);
        auto existingPublisherMap = _publisherMaps.find(publisher.topic());
        if (existingPublisherMap != _publisherMaps.end())
        {
            auto existingPublisher = existingPublisherMap->second.find(publisher.physical_node_info().address());
            if (existingPublisher != existingPublisherMap->second.end())
                existingPublisherMap->second.erase(existingPublisher);
        }
    }

    bool MessageManager::hasPublisher(const champion_rpc::PublisherInfo & publisher)
    {
        lock_guard<mutex> lock(_mutex);
        auto existingPublisherMap = _publisherMaps.find(publisher.topic());
        if (existingPublisherMap == _publisherMaps.end())
            return false;
        else
            return existingPublisherMap->second.find(publisher.physical_node_info().address()) != existingPublisherMap->second.end();
    }

    list<champion_rpc::PublisherInfo> MessageManager::addSubscriber(const champion_rpc::SubscriberInfo & subscriber)
	{
		lock_guard<mutex> lock(_mutex);
        auto existingSubscriberMap = _subscriberMaps.find(subscriber.topic());
        if (existingSubscriberMap == _subscriberMaps.end())
        {
            map<string, champion_rpc::SubscriberInfo> subscriberMap;
            subscriberMap[subscriber.physical_node_info().address()] = subscriber;
            _subscriberMaps[subscriber.topic()] = subscriberMap;
        }
        else
        {
            existingSubscriberMap->second[subscriber.physical_node_info().address()] = subscriber;
        }
        list<champion_rpc::PublisherInfo> publisherList;
        auto publisherMap = _publisherMaps.find(subscriber.topic());
        if (publisherMap != _publisherMaps.end())
        {
            transform(publisherMap->second.begin(), publisherMap->second.end(), back_inserter(publisherList),
                [](map<string, champion_rpc::PublisherInfo>::value_type  & value){return value.second; });
        }
        return publisherList;
	}

    void MessageManager::removeSubscriber(const champion_rpc::SubscriberInfo & subscriber)
    {
        lock_guard<mutex> lock(_mutex);
        auto existingSubscriberMap = _subscriberMaps.find(subscriber.topic());
        if (existingSubscriberMap != _subscriberMaps.end())
        {
            auto existingSubscriber = existingSubscriberMap->second.find(subscriber.physical_node_info().address());
            if (existingSubscriber != existingSubscriberMap->second.end())
                existingSubscriberMap->second.erase(existingSubscriber);
        }
    }

    bool MessageManager::hasSubscriber(const champion_rpc::SubscriberInfo & subscriber)
    {
        lock_guard<mutex> lock(_mutex);
        auto existingSubscriberMap = _subscriberMaps.find(subscriber.topic());
        if (existingSubscriberMap == _subscriberMaps.end())
            return false;
        else
            return existingSubscriberMap->second.find(subscriber.physical_node_info().address()) != existingSubscriberMap->second.end();
    }
}
