#include <atomic>
#include <champion/champion.h>
#include <test_message.pb.h>
#include <iostream>

using namespace std;

class PubNode : public champion::Node
{
private:
	string m_message;
		string m_topic;
public:
	PubNode(string m,string t)
	{
            //atomic_init(&_cancellationRequested, false);
            _cancellationRequested = false;
						m_message = m;
						m_topic = t;
	}

	future<void> onStart() override
	{
		return async(launch::async, [this](){
            _publisher = champion::advertise<champion_example::TestMessage>(m_topic, "TheGreatPublisher1897", [this](string topic, bool ready){
                if (ready)
                    _pubReady.set_value(ready);
            });
			_publishThread = make_shared<thread>(&PubNode::publishMessage, this);
		});
	}

	future<void> onStop() override
	{
		return async(launch::async, [this](){
			_cancellationRequested = true;
			_publishThread->join();
			std::cout << "publish thread stopped." << std::endl;
		});
	}
private:
    std::atomic<bool> _cancellationRequested;

	shared_ptr<champion::Publisher<champion_example::TestMessage>> _publisher;

	shared_ptr<thread> _publishThread;

    promise<bool> _pubReady;

	void publishMessage()
	{
        std::cout << "wait for incoming subs..." << std::endl;
        //_pubReady.get_future().wait();
        std::cout << "sub arrives" << std::endl;;
		//this_thread::sleep_for(chrono::milliseconds(1000));
		auto count = 0;
		while  (!_cancellationRequested)
		{
			count++;
            std::cout << "publish test string: "<< m_message << std::endl;
			champion_example::TestMessage message;
			message.set_count(count);
			message.set_detail(m_message);
            // sync:
             _publisher->publish(message);
            // async:
            //_publisher->asyncPublish(message);
			//this_thread::sleep_for(chrono::milliseconds(1000));
		}
        std::cout << "publish thread canceled." << std::endl;
	}
};

int main(int argc, char **argv)
{
    //champion::set_gpr_navite_log_function("./example_publisher");
		string champion_message = string(argv[1]);
		string champion_topic = string(argv[2]);
	champion::init("localhost:23333", "localhost:30000", "example_publisher_physical");
	auto node = make_shared<PubNode>(champion_message,champion_topic);
	champion::addNode(node);
	champion::spin();
    return 0;
}
