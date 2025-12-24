#include "gtest/gtest.h"

#include "apollo/net/protobuf_message.hpp"

#include <google/protobuf/any.pb.h>

using namespace apollo::net;

TEST(ProtobufCodecTest, EncodeDecodeAny) {
    google::protobuf::Any any;
    any.set_type_url("type.googleapis.com/test.Any");
    any.set_value("hello");

    Buffer buffer;
    ASSERT_TRUE(ProtobufCodec::Encode(any, /*msgType=*/123, buffer));

    auto [type, payload] = ProtobufCodec::Decode(buffer);
    ASSERT_EQ(type, 123u);
    ASSERT_FALSE(payload.empty());

    google::protobuf::Any decoded;
    ASSERT_TRUE(decoded.ParseFromArray(payload.data(), static_cast<int>(payload.size())));
    EXPECT_EQ(decoded.type_url(), any.type_url());
    EXPECT_EQ(decoded.value(), any.value());
}

TEST(ProtobufMessageWrapperTest, SerializeDeserializeRoundTrip) {
    google::protobuf::Any any;
    any.set_type_url("type.googleapis.com/test.Any");
    any.set_value("world");

    ProtobufMessageWrapper<google::protobuf::Any> msg(any);

    Buffer buffer;
    ASSERT_TRUE(msg.Serialize(buffer));

    ProtobufMessageWrapper<google::protobuf::Any> msg2;
    ASSERT_TRUE(msg2.Deserialize(buffer));

    EXPECT_EQ(msg2.GetTypedMessage()->type_url(), any.type_url());
    EXPECT_EQ(msg2.GetTypedMessage()->value(), any.value());
}
