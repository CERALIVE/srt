/*
 * CERALIVE periodic NAK reorder-tolerance regression.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. https://mozilla.org/MPL/2.0/
 */
#include "platform_sys.h"
#include "srt.h"
#include "test_env.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <thread>
#include <vector>

#ifndef _WIN32
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define closesocket close
#endif

namespace {
using Clock = std::chrono::steady_clock;
using Datagram = std::vector<char>;
const uint32_t sequence_mask = 0x7fffffff;

uint32_t word(const Datagram& packet, size_t offset)
{
    uint32_t value;
    std::memcpy(&value, &packet[offset], sizeof value);
    return ntohl(value);
}

struct ControlFrame
{
    Datagram packet;
    Clock::time_point at;
    bool from_receiver;

    bool names(uint32_t sequence) const
    {
        if (!from_receiver || (word(packet, 0) >> 16) != 0x8003)
            return false;
        for (size_t i = 16; i + 4 <= packet.size(); i += 4)
        {
            const uint32_t first = word(packet, i);
            uint32_t last = first;
            if (first & 0x80000000)
            {
                i += 4;
                if (i + 4 > packet.size())
                    return false;
                last = word(packet, i);
            }
            if (((sequence - (first & sequence_mask)) & sequence_mask)
                <= ((last - (first & sequence_mask)) & sequence_mask))
                return true;
        }
        return false;
    }
};

// One ephemeral loopback UDP socket; only this worker touches the wire trace.
class ReorderingProxy
{
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in receiver_, caller_ = {};
    std::atomic<bool> stopping_{false};
    std::thread worker_;
    std::promise<void> done_;
    Datagram held_;
    std::vector<Datagram> buffered_;
    int repeated_loss_ = 0;

    void forward(const Datagram& packet, const sockaddr_in& destination)
    {
        EXPECT_EQ(sendto(socket_, packet.data(), int(packet.size()), 0,
                         reinterpret_cast<const sockaddr*>(&destination), sizeof destination), int(packet.size()));
    }

    void data(const Datagram& packet)
    {
        const uint32_t sequence = word(packet, 0);
        if (isn == sequence_mask + 1)
            isn = sequence;
        const uint32_t offset = (sequence - isn) & sequence_mask;
        if (offset == 150)
            return; // Genuine loss: discard original AND all retransmissions.
        if (offset == 100 && held_.empty() && released == Clock::time_point())
        {
            held_ = packet;
            gap = Clock::now();
            return;
        }
        if (!held_.empty() && (offset >= 141 || offset == 100))
        {
            buffered_.push_back(packet);
            return;
        }
        forward(packet, receiver_);
        if (offset >= 101 && offset <= 140 && released == Clock::time_point())
            ++reorder_distance;
        if (offset == 199)
            last_data = Clock::now();
    }

    void run()
    {
        while (!stopping_)
        {
            if (!held_.empty() && Clock::now() - gap >= std::chrono::milliseconds(400))
            {
                forward(held_, receiver_);
                released = Clock::now();
                held_.clear();
                for (const auto& packet : buffered_)
                    data(packet);
                buffered_.clear();
            }
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(socket_, &readable);
            timeval timeout = {0, 5000}; // Bounded stop and hold-deadline wakeup, not a test sleep.
            const int ready = select(int(socket_) + 1, &readable, NULL, NULL, &timeout);
            if (ready == 0)
                continue;
            if (ready < 0)
            {
                ADD_FAILURE() << "proxy select failed";
                return;
            }
            Datagram packet(1500);
            sockaddr_in source = {};
            socklen_t length = sizeof source;
            const int size = recvfrom(socket_, packet.data(), int(packet.size()), 0,
                                      reinterpret_cast<sockaddr*>(&source), &length);
            if (size < 16)
            {
                ADD_FAILURE() << "proxy received invalid datagram length " << size;
                return;
            }
            packet.resize(size);
            const bool reverse = source.sin_port == receiver_.sin_port;
            if (!reverse)
                caller_ = source;
            if (word(packet, 0) & 0x80000000)
            {
                controls.push_back({packet, Clock::now(), reverse});
                forward(packet, reverse ? caller_ : receiver_);
                if (last_data != Clock::time_point() && controls.back().names((isn + 150) & sequence_mask)
                    && ++repeated_loss_ == 3)
                {
                    done_.set_value();
                }
            }
            else if (!reverse)
                data(packet);
        }
    }

public:
    sockaddr_in address = {};
    uint32_t isn = sequence_mask + 1;
    int reorder_distance = 0;
    Clock::time_point gap, released, last_data;
    std::vector<ControlFrame> controls;

    explicit ReorderingProxy(sockaddr_in receiver) : receiver_(receiver) {}
    ~ReorderingProxy()
    {
        stop();
        if (socket_ != INVALID_SOCKET)
        {
            EXPECT_EQ(closesocket(socket_), 0);
        }
    }
    void start()
    {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        ASSERT_NE(socket_, INVALID_SOCKET);
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_EQ(::bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof address), 0);
        socklen_t length = sizeof address;
        ASSERT_EQ(getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &length), 0);
        worker_ = std::thread(&ReorderingProxy::run, this);
    }
    bool wait()
    {
        return done_.get_future().wait_for(std::chrono::seconds(3)) == std::future_status::ready;
    }
    void stop()
    {
        stopping_ = true;
        if (worker_.joinable())
            worker_.join();
    }
};

class PeriodicNakGate : public testing::TestWithParam<bool> {};

TEST_P(PeriodicNakGate, ReorderingAndGenuineLoss)
{
    // Given live loopback peers, a 50-packet TTL, and the same wire proxy in both arms.
    srt::TestInit init;
    MAKE_UNIQUE_SOCK(listener, "listener", srt_create_socket());
    MAKE_UNIQUE_SOCK(caller, "caller", srt_create_socket());
    const SRT_TRANSTYPE live = SRTT_LIVE;
    const int latency = 1000, ttl = 50, timeout = 2000;
    const bool enabled = GetParam(), nakreport = true;
    for (SRTSOCKET socket : {SRTSOCKET(listener), SRTSOCKET(caller)})
    {
        ASSERT_EQ(srt_setsockflag(socket, SRTO_TRANSTYPE, &live, sizeof live), 0);
        ASSERT_EQ(srt_setsockflag(socket, SRTO_LATENCY, &latency, sizeof latency), 0);
        ASSERT_EQ(srt_setsockflag(socket, SRTO_SNDTIMEO, &timeout, sizeof timeout), 0);
        ASSERT_EQ(srt_setsockflag(socket, SRTO_RCVTIMEO, &timeout, sizeof timeout), 0);
        sockaddr_in local = {};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_EQ(srt_bind(socket, reinterpret_cast<const sockaddr*>(&local), sizeof local), 0);
    }
    ASSERT_EQ(srt_setsockflag(listener, SRTO_LOSSMAXTTL, &ttl, sizeof ttl), 0);
    ASSERT_EQ(srt_setsockflag(listener, SRTO_PERIODICNAKGATE, &enabled, sizeof enabled), 0);
    ASSERT_EQ(srt_setsockflag(listener, SRTO_NAKREPORT, &nakreport, sizeof nakreport), 0);
    ASSERT_EQ(srt_listen(listener, 1), 0);
    sockaddr_in destination = {};
    int length = sizeof destination;
    ASSERT_EQ(srt_getsockname(listener, reinterpret_cast<sockaddr*>(&destination), &length), 0);
    ReorderingProxy proxy(destination);
    proxy.start();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(srt_connect(caller, reinterpret_cast<const sockaddr*>(&proxy.address), sizeof proxy.address), 0);
    MAKE_UNIQUE_SOCK(accepted, "accepted", srt_accept(listener, NULL, NULL));

    // When 200 ordered submissions are reordered ONLY on the wire; #150 is truly lost.
    for (int i = 0; i < 200; ++i)
    {
        SRT_MSGCTRL control = srt_msgctrl_default;
        ASSERT_EQ(srt_sendmsg2(caller, reinterpret_cast<const char*>(&i), sizeof i, &control), int(sizeof i));
    }
    const bool observed_repeats = proxy.wait();
    proxy.stop(); // Join before reading the trace; no shared observation races.

    // Then the control must detect the bug, while the gate preserves the fresh gap.
    ASSERT_TRUE(observed_repeats) << "genuine loss was not repeatedly reported";
    ASSERT_EQ(proxy.reorder_distance, 40);
    ASSERT_GE(proxy.released - proxy.gap, std::chrono::milliseconds(400));
    size_t premature = 0, confirmed = 0;
    for (const auto& frame : proxy.controls)
    {
        if (frame.at < proxy.released && frame.names((proxy.isn + 100) & sequence_mask))
            ++premature;
        if (frame.at >= proxy.last_data && frame.names((proxy.isn + 150) & sequence_mask))
            ++confirmed;
    }
    if (enabled)
        EXPECT_EQ(premature, 0u);
    else
        EXPECT_GT(premature, 0u);
    // With DATA stopped after #199, at least two of these three reports are timer-driven.
    EXPECT_GE(confirmed, 3u);
    for (int i = 0; i < 150; ++i)
    {
        char payload[1316];
        int received = -1;
        ASSERT_EQ(srt_recvmsg(accepted, payload, sizeof payload), int(sizeof received));
        std::memcpy(&received, payload, sizeof received);
        ASSERT_EQ(received, i); // Includes the recovered #100 before any TLPKTDROP.
    }
    accepted.close();
    caller.close();
    listener.close();
}

INSTANTIATE_TEST_SUITE_P(Wire, PeriodicNakGate, testing::Bool());
} // namespace
