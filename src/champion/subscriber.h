#ifndef CHAMPION_SUBSCRIBER_H
#define CHAMPION_SUBSCRIBER_H

#include "subscriber_interface.h"
#include "physical_node.h"

using namespace std;

namespace champion
{
	template<class TMessage>
    class Subscriber : public ISubscriber
	{
	public:
		Subscriber(const string & topic, 
            function<void(const TMessage &, const champion_rpc::PublisherNodeInfo &)> callback, 
            bool b_req_pub_info,
            shared_ptr<PhysicalNode> physical_node)
            : ISubscriber(topic, b_req_pub_info), _callback(callback), _physical_node(physical_node)
		{
			static_assert(is_base_of<::google::protobuf::Message, TMessage>::value
				|| is_base_of<string, TMessage>::value, "Unsupported message type");
		}

        void handleRawMessage(const string & messageContent, const champion_rpc::PublisherNodeInfo *pub_info = NULL) override
		{
			if (typeid(TMessage) == typeid(string))
			{
				handleMessage((const TMessage &)messageContent, pub_info);
			}
			else
			{
				TMessage message;
				((::google::protobuf::Message *) &message)->ParseFromString(messageContent);
				handleMessage(message, pub_info);
			}
			
		}

        void handleMessage(const TMessage & message, const champion_rpc::PublisherNodeInfo *pub_info = NULL)
        {
            champion_rpc::PublisherNodeInfo pub;
            if (_b_req_pub_info && pub_info)
            {
                pub = *pub_info;
            }
            _physical_node->QueueTask(_callback, message, pub); // todo(lishen): handle exception
        }

	private:
		function<void(const TMessage &, const champion_rpc::PublisherNodeInfo &)> _callback;
        shared_ptr<PhysicalNode> _physical_node;
	};
}

#endif