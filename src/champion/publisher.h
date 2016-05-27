#ifndef CHAMPION_PUBLISHER_H
#define CHAMPION_PUBLISHER_H

#include <map>
#include <list>
#include "publisher_interface.h"
#include "physical_node.h"
#include "serialization_helper.h"

using namespace std;

namespace champion
{
	template<class TMessage>
	class Publisher : public IPublisher
	{
	public:
        Publisher(const string & topic, const string & publisher_info, shared_ptr<PhysicalNode> physical_node)
            : IPublisher(topic, publisher_info), _physical_node(physical_node)
        { 
        }

        shared_ptr<map<string, champion_rpc::Status>> publish(const TMessage &message, int timeout_mseconds = -1)
		{
            auto content = serializeToString(message);
            return _physical_node->publish(_topic, content, _pub_info, timeout_mseconds);
		}

        // return value: hold the environment built to async pub to remote subs
        shared_ptr<shared_future<shared_ptr<map<string, champion_rpc::Status>>>> asyncPublish(const TMessage &message, int timeout_mseconds = -1)
        {
            auto content = serializeToString(message);
            auto f_res_ptr = make_shared<shared_future<shared_ptr<map<string, champion_rpc::Status>>>>(
                _physical_node->asyncPublish(_topic, content, _pub_info, timeout_mseconds).share());
            updateDedicatedResults(f_res_ptr);
            return f_res_ptr;
        }

    private:
        shared_ptr<PhysicalNode> _physical_node;

    private:
        list<shared_ptr<shared_future<shared_ptr<map<string, champion_rpc::Status>>>>> _dedicated_results;
        mutex _results_lock;

        void updateDedicatedResults(shared_ptr<shared_future<shared_ptr<map<string, champion_rpc::Status>>>> new_ptr)
        {
            lock_guard<mutex> l(_results_lock);
            _dedicated_results.remove_if([this](shared_ptr<shared_future<shared_ptr<map<string, champion_rpc::Status>>>> pts){
                if (pts.use_count() <= 2 && pts->wait_for(chrono::seconds(0)) != future_status::ready) {
                    _physical_node->QueueTask([pts](){ pts->wait(); });
                }
                return pts.use_count() <= 2;
            });
            _dedicated_results.push_back(new_ptr);
        }
	};
}

#endif