#ifndef CHAMPION_PUBLISHER_INTERFACE_H
#define CHAMPION_PUBLISHER_INTERFACE_H

#include <string>

using namespace std;

namespace champion
{
    class IPublisher
    {
    public:
        IPublisher(const string & topic, const string & publisher_info)
            : _topic(topic), _pub_info(publisher_info)
        {
        }
        const string & getTopic()
        {
            return _topic;
        }
        const string & getInfo()
        {
            return _pub_info;
        }
    protected:
        string _topic;
        string _pub_info;
    };
}
#endif