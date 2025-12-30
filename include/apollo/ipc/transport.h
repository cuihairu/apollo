#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace apollo {
namespace ipc {

enum class ChannelState : uint8_t;

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool connect(const std::string& address) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;

    virtual bool send(const void* data, size_t size) = 0;
    virtual size_t receive(void* buffer, size_t size) = 0;

    virtual void sendAsync(std::vector<uint8_t> data,
                           std::function<void(bool)> callback) = 0;
    virtual void receiveAsync(std::function<void(std::vector<uint8_t>)> callback) = 0;

    virtual ChannelState getState() const = 0;
    virtual std::string getLastError() const = 0;
    virtual int getFd() const = 0;

    virtual size_t getAvailable() const = 0;
    virtual size_t getWritable() const = 0;
};

} // namespace ipc
} // namespace apollo

