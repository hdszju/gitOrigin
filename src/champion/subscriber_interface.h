#ifndef CHAMPION_SUBSCRIBER_INTERFACE_H
#define CHAMPION_SUBSCRIBER_INTERFACE_H

#include <string>

using namespace std;

namespace champion
{
    class ISubscriber
    {
    public:
        ISubscriber(const string & topic, bool b_req_pub_info)
            : _topic(topic), _b_req_pub_info(b_req_pub_info)
        {
        }

        virtual void handleRawMessage(const string & messageContent, const champion_rpc::PublisherNodeInfo *pub_info = NULL) = 0;

        const string & getTopic()
        {
            return _topic;
        }

        bool reqPubInfo()
        {
            return _b_req_pub_info;
        }

    protected:
        string _topic;
        bool _b_req_pub_info;
    };
}
#endif