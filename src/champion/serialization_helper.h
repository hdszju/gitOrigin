#ifndef CHAMPION_SERIALIZATION_HELPER_H
#define CHAMPION_SERIALIZATION_HELPER_H

#include <champion_rpc.grpc.pb.h>

using namespace std;

namespace champion {
    template<class T>
    string serializeToString(const T & object)
    {
        // todo: support more type and serialize to binary (hds)
        static_assert(is_base_of<::google::protobuf::Message, T>::value
            || is_base_of<string, T>::value, "Unsupported type");

        if (typeid(T) == typeid(string))
            return ((const string &)object);
        else if (is_base_of<::google::protobuf::Message, T>::value)
            return ((const ::google::protobuf::Message &)object).SerializeAsString();
    }

    template<class T>
    void parseFromString(const string & data, T * object)
    {
        // todo: support more type (hds)
        static_assert(is_base_of<::google::protobuf::Message, T>::value
            || is_base_of<string, T>::value, "Unsupported type");

        if (typeid(T) == typeid(string))
            *((string *)object) = data;
        else if (is_base_of<::google::protobuf::Message, T>::value)
            ((::google::protobuf::Message *)object)->ParseFromString(data);
    }
}
#endif