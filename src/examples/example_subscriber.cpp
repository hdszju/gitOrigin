#include <champion/champion.h>
#include <test_message.pb.h>
#include <iostream>

using namespace std;

class SubNode : public champion::Node
{
private:
	string topic;
public:
	SubNode(string t)
	{
		topic = t;
	}
	future<void> onStart() override
	{
		return async(launch::async, [this](){
			champion::subscribe<champion_example::TestMessage>(topic, [](const champion_example::TestMessage & message, const champion_rpc::PublisherNodeInfo &pub_info){
				std::cout << "receive message: " << message.count() << " " << message.detail() << " from" << pub_info.publisher_name() << std::endl;
			}, true);
		});
	}

	future<void> onStop() override
	{
		return async(launch::async, []{});
	}
};

int main()
{
    //champion::set_gpr_navite_log_function("./example_subscriber");
	champion::init("localhost:23333", "localhost:30001", "example_subscriber_physical");
	auto node = make_shared<SubNode>("test_topic");
	champion::addNode(node);
	champion::spin();
    return 0;
}
