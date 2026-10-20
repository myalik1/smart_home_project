#pragma once

class IEmulator {
public:
    virtual ~IEmulator() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual void tick() = 0;
};
