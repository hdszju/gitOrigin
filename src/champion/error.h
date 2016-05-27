#ifndef CHAMPION_ERROR_H
#define CHAMPION_ERROR_H

#include <stdexcept>

using namespace std;

namespace champion
{
	class initialize_again : public logic_error
	{
	public:
		explicit initialize_again(const string & message) : logic_error(message){}

		explicit initialize_again(const char * message)	: logic_error(message){}
	};

	class champion_not_initialized : public logic_error
	{
	public:
		explicit champion_not_initialized(const string & message) : logic_error(message){}

		explicit champion_not_initialized(const char * message)	: logic_error(message){}
	};

    class initialize_error : public logic_error
    {
    public:
        explicit initialize_error(const string & message) : logic_error(message){}

        explicit initialize_error(const char * message) : logic_error(message){}
    };

    class invalid_operation : public logic_error
    {
    public:
        explicit invalid_operation(const string & message) : logic_error(message){}

        explicit invalid_operation(const char * message) : logic_error(message){}
    };
}

#endif