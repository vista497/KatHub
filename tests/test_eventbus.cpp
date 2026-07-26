#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QThread>
#include <QAtomicInt>
#include <QMutex>
#include <QWaitCondition>

#include "SignalHub.h"

using namespace KatHub;

// ============================================================================
// Fixture: provides a fresh SignalHub for each test.
// ============================================================================
class SignalHubTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        hub = std::make_unique<SignalHub>();
    }

    void TearDown() override
    {
        hub.reset();
    }

    /// Helper: returns a simple payload with one string key.
    static QJsonObject makePayload(const QString &key, const QString &value)
    {
        QJsonObject obj;
        obj[key] = value;
        return obj;
    }

    std::unique_ptr<SignalHub> hub;
};

// ============================================================================
// 1. Publish-subscribe round-trip — basic sanity.
// ============================================================================
TEST_F(SignalHubTest, RoundTrip_SingleSubscriber)
{
    bool called = false;
    QJsonObject received;

    hub->subscribe(QStringLiteral("test.topic"),
                   [&](const QJsonObject &data) {
                       called = true;
                       received = data;
                   });

    QJsonObject payload = makePayload(QStringLiteral("msg"), QStringLiteral("hello"));
    hub->publish(QStringLiteral("test.topic"), payload);

    EXPECT_TRUE(called);
    EXPECT_EQ(received.value(QStringLiteral("msg")).toString().toStdString(),
              "hello");
}

// ============================================================================
// 2. Multiple subscribers on the same topic all receive the event.
// ============================================================================
TEST_F(SignalHubTest, MultipleSubscribers_AllCalled)
{
    QAtomicInt callCount = 0;

    for (int i = 0; i < 5; ++i) {
        hub->subscribe(QStringLiteral("multi"),
                       [&](const QJsonObject &) {
                           callCount.fetchAndAddRelaxed(1);
                       });
    }

    hub->publish(QStringLiteral("multi"), QJsonObject{});

    EXPECT_EQ(callCount.loadRelaxed(), 5);
}

// ============================================================================
// 3. Unsubscribe stops further delivery.
// ============================================================================
TEST_F(SignalHubTest, Unsubscribe_StopsDelivery)
{
    QAtomicInt callCount = 0;

    int handle = hub->subscribe(QStringLiteral("topic"),
                                [&](const QJsonObject &) {
                                    callCount.fetchAndAddRelaxed(1);
                                });

    // First publish — should be delivered.
    hub->publish(QStringLiteral("topic"), QJsonObject{});
    EXPECT_EQ(callCount.loadRelaxed(), 1);

    // Unsubscribe.
    hub->unsubscribe(QStringLiteral("topic"), handle);

    // Second publish — should NOT be delivered.
    hub->publish(QStringLiteral("topic"), QJsonObject{});
    EXPECT_EQ(callCount.loadRelaxed(), 1); // still 1
}

// ============================================================================
// 4. Unsubscribe with an invalid handle is a no-op (does not crash).
// ============================================================================
TEST_F(SignalHubTest, Unsubscribe_WrongHandle_NoCrash)
{
    hub->subscribe(QStringLiteral("topic"),
                   [](const QJsonObject &) {});

    EXPECT_NO_THROW(hub->unsubscribe(QStringLiteral("topic"), 99999));
}

// ============================================================================
// 5. Unsubscribe from a topic with no subscribers is a no-op.
// ============================================================================
TEST_F(SignalHubTest, Unsubscribe_NonexistentTopic_NoCrash)
{
    EXPECT_NO_THROW(hub->unsubscribe(QStringLiteral("ghost"), 0));
}

// ============================================================================
// 6. Event payload integrity — complex JSON survives round-trip.
// ============================================================================
TEST_F(SignalHubTest, PayloadIntegrity_ComplexJson)
{
    QJsonObject received;
    hub->subscribe(QStringLiteral("data"),
                   [&](const QJsonObject &data) {
                       received = data;
                   });

    QJsonObject payload;
    payload[QStringLiteral("int")]    = 42;
    payload[QStringLiteral("double")] = 3.14;
    payload[QStringLiteral("bool")]   = true;
    payload[QStringLiteral("str")]    = QStringLiteral("привет");
    payload[QStringLiteral("null")]   = QJsonValue();

    QJsonObject nested;
    nested[QStringLiteral("key")] = QStringLiteral("value");
    payload[QStringLiteral("obj")] = nested;

    QJsonArray arr{1, 2, 3};
    payload[QStringLiteral("arr")] = arr;

    hub->publish(QStringLiteral("data"), payload);

    EXPECT_EQ(received.value(QStringLiteral("int")).toInt(), 42);
    EXPECT_DOUBLE_EQ(received.value(QStringLiteral("double")).toDouble(), 3.14);
    EXPECT_TRUE(received.value(QStringLiteral("bool")).toBool());
    EXPECT_EQ(received.value(QStringLiteral("str")).toString().toStdString(),
              "привет");
    EXPECT_TRUE(received.value(QStringLiteral("null")).isNull());
    EXPECT_EQ(received.value(QStringLiteral("obj")).toObject()
                  .value(QStringLiteral("key")).toString().toStdString(),
              "value");
    EXPECT_EQ(received.value(QStringLiteral("arr")).toArray().size(), 3);
}

// ============================================================================
// 7. Qt signal signalPublished(topic, data) is emitted after publish.
// ============================================================================
TEST_F(SignalHubTest, SignalPublished_Emitted)
{
    QSignalSpy spy(hub.get(), &SignalHub::signalPublished);

    QJsonObject payload = makePayload(QStringLiteral("x"), QStringLiteral("1"));
    hub->publish(QStringLiteral("signals"), payload);

    ASSERT_EQ(spy.count(), 1);

    const QList<QVariant> &args = spy.at(0);
    EXPECT_EQ(args.at(0).toString().toStdString(), "signals");

    QJsonObject emittedData = args.at(1).toJsonObject();
    EXPECT_EQ(emittedData.value(QStringLiteral("x")).toString().toStdString(), "1");
}

// ============================================================================
// 8. SignalPublished is emitted even when no subscribers exist.
// ============================================================================
TEST_F(SignalHubTest, SignalPublished_EmittedWithNoSubscribers)
{
    QSignalSpy spy(hub.get(), &SignalHub::signalPublished);

    hub->publish(QStringLiteral("lonely"), QJsonObject{});

    EXPECT_EQ(spy.count(), 1);
}

// ============================================================================
// 9. Separate topics are isolated.
// ============================================================================
TEST_F(SignalHubTest, MultipleTopics_Isolated)
{
    QAtomicInt countA = 0;
    QAtomicInt countB = 0;

    hub->subscribe(QStringLiteral("A"), [&](const QJsonObject &) { countA.fetchAndAddRelaxed(1); });
    hub->subscribe(QStringLiteral("B"), [&](const QJsonObject &) { countB.fetchAndAddRelaxed(1); });

    hub->publish(QStringLiteral("A"), QJsonObject{});

    EXPECT_EQ(countA.loadRelaxed(), 1);
    EXPECT_EQ(countB.loadRelaxed(), 0);

    hub->publish(QStringLiteral("B"), QJsonObject{});

    EXPECT_EQ(countA.loadRelaxed(), 1);
    EXPECT_EQ(countB.loadRelaxed(), 1);
}

// ============================================================================
// 10. Subscribe handles are monotonically increasing.
// ============================================================================
TEST_F(SignalHubTest, Subscribe_ReturnsIncreasingHandles)
{
    int h1 = hub->subscribe(QStringLiteral("t"), [](const QJsonObject &) {});
    int h2 = hub->subscribe(QStringLiteral("t"), [](const QJsonObject &) {});
    int h3 = hub->subscribe(QStringLiteral("t2"), [](const QJsonObject &) {});

    EXPECT_LT(h1, h2);
    EXPECT_LT(h2, h3);
}

// ============================================================================
// 11. After unsubscribe, only the removed subscriber stops receiving;
//     others on the same topic still get deliveries.
// ============================================================================
TEST_F(SignalHubTest, Unsubscribe_DoesNotAffectOtherSubscribers)
{
    QAtomicInt countA = 0;
    QAtomicInt countB = 0;

    int hA = hub->subscribe(QStringLiteral("topic"),
                            [&](const QJsonObject &) { countA.fetchAndAddRelaxed(1); });
    hub->subscribe(QStringLiteral("topic"),
                   [&](const QJsonObject &) { countB.fetchAndAddRelaxed(1); });

    hub->unsubscribe(QStringLiteral("topic"), hA);

    hub->publish(QStringLiteral("topic"), QJsonObject{});

    EXPECT_EQ(countA.loadRelaxed(), 0); // removed
    EXPECT_EQ(countB.loadRelaxed(), 1); // still active
}

// ============================================================================
// 12. Thread-safety: concurrent publish from multiple threads.
// ============================================================================
TEST_F(SignalHubTest, ThreadSafety_ConcurrentPublish)
{
    QAtomicInt totalCalls = 0;

    // One subscriber counting all deliveries.
    hub->subscribe(QStringLiteral("ts"),
                   [&](const QJsonObject &) {
                       totalCalls.fetchAndAddRelaxed(1);
                   });

    constexpr int kThreads      = 4;
    constexpr int kPublishEach  = 100;

    QList<QThread *> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        auto *thread = QThread::create([this]() {
            for (int i = 0; i < kPublishEach; ++i) {
                hub->publish(QStringLiteral("ts"), QJsonObject{});
            }
        });
        threads.append(thread);
        thread->start();
    }

    for (auto *t : threads) {
        t->wait();
        delete t;
    }

    EXPECT_EQ(totalCalls.loadRelaxed(), kThreads * kPublishEach);
}

// ============================================================================
// 13. Thread-safety: concurrent subscribe while publishing.
// ============================================================================
TEST_F(SignalHubTest, ThreadSafety_ConcurrentSubscribeWhilePublishing)
{
    QAtomicInt stopped = 0;

    // Publisher thread
    auto *publisher = QThread::create([this, &stopped]() {
        while (!stopped.loadRelaxed()) {
            hub->publish(QStringLiteral("hot"), QJsonObject{});
        }
    });

    // Subscriber threads — hammer subscribe/unsubscribe
    QList<QThread *> subs;
    for (int i = 0; i < 4; ++i) {
        subs.append(QThread::create([this, &stopped]() {
            for (int j = 0; j < 50; ++j) {
                int h = hub->subscribe(QStringLiteral("hot"),
                                       [](const QJsonObject &) {});
                hub->unsubscribe(QStringLiteral("hot"), h);
            }
        }));
    }

    publisher->start();
    for (auto *t : subs)
        t->start();

    for (auto *t : subs) {
        t->wait();
        delete t;
    }

    stopped.storeRelaxed(1);
    publisher->wait();
    delete publisher;

    // No crash = pass
    SUCCEED();
}

// ============================================================================
// 14. Re-entrant subscribe from within a callback does not deadlock.
// ============================================================================
TEST_F(SignalHubTest, Reentrant_SubscribeFromCallback)
{
    QAtomicInt callCount = 0;

    hub->subscribe(QStringLiteral("re"),
                   [&](const QJsonObject &) {
                       callCount.fetchAndAddRelaxed(1);
                       // Re-entrant subscribe — must not deadlock.
                       hub->subscribe(QStringLiteral("re"),
                                      [&](const QJsonObject &) {
                                          callCount.fetchAndAddRelaxed(1);
                                      });
                   });

    hub->publish(QStringLiteral("re"), QJsonObject{});

    // First publish: only the original subscriber fires (snapshot was taken
    // before the re-entrant subscribe could add the second one).
    EXPECT_EQ(callCount.loadRelaxed(), 1);

    // Second publish: now both fire.
    callCount.storeRelaxed(0);
    hub->publish(QStringLiteral("re"), QJsonObject{});
    EXPECT_EQ(callCount.loadRelaxed(), 2);
}

// ============================================================================
// 15. Re-entrant unsubscribe from within a callback does not deadlock.
// ============================================================================
TEST_F(SignalHubTest, Reentrant_UnsubscribeFromCallback)
{
    QAtomicInt callCount = 0;

    int h = hub->subscribe(QStringLiteral("re"),
                           [&](const QJsonObject &) {
                               callCount.fetchAndAddRelaxed(1);
                               // Unsubscribe self from within callback.
                               hub->unsubscribe(QStringLiteral("re"), h);
                           });

    hub->subscribe(QStringLiteral("re"),
                   [&](const QJsonObject &) {
                       callCount.fetchAndAddRelaxed(1);
                   });

    // First publish: both fire (snapshot taken before callbacks run).
    hub->publish(QStringLiteral("re"), QJsonObject{});
    EXPECT_EQ(callCount.loadRelaxed(), 2);

    // Second publish: only second subscriber fires (first unsubscribed itself).
    callCount.storeRelaxed(0);
    hub->publish(QStringLiteral("re"), QJsonObject{});
    EXPECT_EQ(callCount.loadRelaxed(), 1);
}

// ============================================================================
// 16. Publish to a topic with no subscribers — no crash, signal still emitted.
// ============================================================================
TEST_F(SignalHubTest, Publish_NoSubscribers_NoCrash)
{
    QSignalSpy spy(hub.get(), &SignalHub::signalPublished);

    EXPECT_NO_THROW(hub->publish(QStringLiteral("nobody"), QJsonObject{}));

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString().toStdString(), "nobody");
}

// ============================================================================
// 17. Multiple unsubscribes on the same handle are harmless.
// ============================================================================
TEST_F(SignalHubTest, DoubleUnsubscribe_NoCrash)
{
    int h = hub->subscribe(QStringLiteral("t"), [](const QJsonObject &) {});

    hub->unsubscribe(QStringLiteral("t"), h);
    EXPECT_NO_THROW(hub->unsubscribe(QStringLiteral("t"), h));
}
