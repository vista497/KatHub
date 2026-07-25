#include <gtest/gtest.h>

#include "httplib.h"

// ---------------------------------------------------------------------------
// Integration test: hits GET /api/status on a running KatHub server.
// Automatically skipped if the server is not reachable.
// ---------------------------------------------------------------------------
class StatusEndpointTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        client = std::make_unique<httplib::Client>("http://localhost:8080");
        client->set_connection_timeout(1, 0); // 1 second
        client->set_read_timeout(1, 0);
    }

    // Quick probe to see if the server is listening.
    bool serverAvailable()
    {
        auto res = client->Get("/api/status");
        return res != nullptr;
    }

    std::unique_ptr<httplib::Client> client;
};

TEST_F(StatusEndpointTest, GetStatusReturnsOk)
{
    if (!serverAvailable()) {
        GTEST_SKIP() << "Server not running on localhost:8080";
    }

    auto res = client->Get("/api/status");
    ASSERT_NE(res, nullptr) << "No response from /api/status";
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("\"status\":\"ok\""), std::string::npos)
        << "Response body: " << res->body;
}
