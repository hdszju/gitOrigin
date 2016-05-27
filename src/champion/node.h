#ifndef CHAMPION_CHAMPION_H
#define CHAMPION_CHAMPION_H

#include <future>

using namespace std;

namespace champion
{
	class Node
	{
	public:
		virtual future<void> onStart() = 0;
		virtual future<void> onStop() = 0;
	};
}

#endif