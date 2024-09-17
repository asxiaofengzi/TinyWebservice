#include <gtest/gtest.h>
#include "httprequest.h"

TEST(HttpRequestTest, ParseFromUrlencodedTest)
{
  HttpRequest request;

  // Test case 1: Empty body
  request.ParseFromUrlencoded_();
  EXPECT_TRUE(request.post_.empty());

  // Test case 2: Single key-value pair
  request.body_ = "key=value";
  request.ParseFromUrlencoded_();
  EXPECT_EQ(request.post_["key"], "value");

  // Test case 3: Multiple key-value pairs
  request.body_ = "key1=value1&key2=value2&key3=value3";
  request.ParseFromUrlencoded_();
  EXPECT_EQ(request.post_["key1"], "value1");
  EXPECT_EQ(request.post_["key2"], "value2");
  EXPECT_EQ(request.post_["key3"], "value3");

  // Test case 4: Key-value pairs with special characters
  request.body_ = "key%20with%20spaces=value%20with%20spaces";
  request.ParseFromUrlencoded_();
  EXPECT_EQ(request.post_["key with spaces"], "value with spaces");

  // Test case 5: Key-value pairs with URL-encoded characters
  request.body_ = "key%3Dencoded=value%26encoded";
  request.ParseFromUrlencoded_();
  EXPECT_EQ(request.post_["key=encoded"], "value&encoded");
}