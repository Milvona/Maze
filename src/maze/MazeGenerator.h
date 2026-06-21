#pragma once

#include <memory>
#include <vector>

#include "ll/api/event/ListenerBase.h"
#include "ll/api/mod/NativeMod.h"
#include "maze/MazeBuilder.h"

class BlockSource;

namespace maze {

class MazeGenerator {

public:
    static MazeGenerator& getInstance();

    MazeGenerator() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

    bool start(BlockSource& source, MazeOptions options, std::string& error);
    void stop();
    void tick();

private:
    ll::mod::NativeMod&                 mSelf;
    std::vector<ll::event::ListenerPtr> mListeners;
    std::unique_ptr<MazeBuilder>        mBuilder;
    BlockSource*                        mSource{nullptr};
};

} // namespace maze
