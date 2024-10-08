#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <string>
#include <vector>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <glog/logging.h>

namespace core {
    using namespace std;

    template<typename T>
    string get_typeurl() {
        return "type.googleapis.com/" + T::descriptor()->full_name();
    }

    template<typename T>
    string serialize(T msg) {
        int msg_size = msg.ByteSize();     
        int total_size = msg_size + 4;    

        string buf;
        buf.resize(total_size);

        google::protobuf::io::ArrayOutputStream aos(&buf[0], total_size);
        google::protobuf::io::CodedOutputStream coded_output(&aos);
        coded_output.WriteLittleEndian32(msg_size); 
        msg.SerializeToCodedStream(&coded_output);
        
        return move(buf);
    }

    class Decoder {
        public:
        Decoder()          = default;
        virtual int        decode(const string& raw_msg) = 0;
        virtual void       handle() = 0;
    };
    template<typename T>
    class SpecifiedDecoder final : public Decoder {
        public:
        SpecifiedDecoder() = default;
        using func_t = std::function<void(const T*)>;
        void add_callback(func_t func) {
            functions.push_back(func);
        }
        int decode(const string& buf) override {
            int total_bytes_consumed = 0;
            int buf_size = buf.size();
            int offset = 0;

            while (offset + 4 <= buf_size) {
                uint32_t msg_size = 0;
                google::protobuf::io::CodedInputStream coded_input(reinterpret_cast<const uint8_t*>(buf.data() + offset), buf_size - offset);
                coded_input.ReadLittleEndian32(&msg_size);

                if (offset + 4 + msg_size > buf_size) {
                    break; 
                }

                T msg;
                if (!msg.ParseFromArray(buf.data() + offset + 4, msg_size)) {
                    return 0;
                }
                msgs.push_back(move(msg));

                offset += 4 + msg_size;
                total_bytes_consumed = offset;
            }

            return total_bytes_consumed; 
        }
        void handle() override {
            const int size = msgs.size();
            for (int i = 0; i < size; i++) {
                const T* msg = &msgs.front();
                for (auto f: functions)
                    f(msg);
                msgs.pop_front();
            }
        }

        deque<T>            msgs;
        vector<func_t>      functions;
    };
}

#endif
