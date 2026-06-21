#include "maze/MazeGenerator.h"

#include <charconv>
#include <string>
#include <string_view>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"

namespace maze {
namespace {

bool gCommandRegistered = false;

struct EmptyParam {};

struct MazeCommandParam {
    int         mazeSizeX;
    int         mazeSizeZ;
    int         cellSize;
    int         wallHeight;
    std::string wallBlock;
};

struct MazeAdvancedParam {
    int         mazeSizeX;
    int         mazeSizeZ;
    int         cellSize;
    int         wallHeight;
    std::string wallBlock;
    std::string floorBlock;
    std::string topBlock;
    bool        hasExits;
    int         additionalExits;
    bool        hasRoom;
    int         roomSizeX;
    int         roomSizeZ;
    float       erosion;
    bool        closed;
    bool        hollow;
    int         blocksPerTick;
    int         seed;
};

struct StopParam {
    std::string stop;
};

Player* getPlayer(CommandOrigin const& origin) {
    if (origin.getOriginType() != CommandOriginType::Player) {
        return nullptr;
    }
    return static_cast<Player*>(origin.getEntity());
}

MazeOptions optionsFromPlayer(Player& player) {
    auto pos = player.getFeetBlockPos();
    return {
        .originX = pos.x,
        .originY = pos.y,
        .originZ = pos.z,
    };
}

void startMaze(CommandOrigin const& origin, CommandOutput& output, MazeOptions options) {
    auto* player = getPlayer(origin);
    if (player == nullptr) {
        output.error("MazeGenerator can only be used by a player.");
        return;
    }

    auto& source = player->getDimensionBlockSource();

    std::string error;
    if (!MazeGenerator::getInstance().start(source, std::move(options), error)) {
        output.error("MazeGenerator failed: {}", error);
        return;
    }

    output.success("Maze generation started.");
}

void registerCommand() {
    if (gCommandRegistered) {
        return;
    }
    gCommandRegistered = true;

    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "maze",
        "Generate a configurable maze",
        CommandPermissionLevel::GameDirectors
    );

    command.alias("mazegen");

    command.overload<EmptyParam>().execute([](CommandOrigin const& origin, CommandOutput& output, EmptyParam const&) {
        auto* player = getPlayer(origin);
        if (player == nullptr) {
            output.error("MazeGenerator can only be used by a player.");
            return;
        }
        startMaze(origin, output, optionsFromPlayer(*player));
    });

    command.overload<MazeCommandParam>()
        .required("mazeSizeX")
        .required("mazeSizeZ")
        .required("cellSize")
        .required("wallHeight")
        .required("wallBlock")
        .execute([](CommandOrigin const& origin, CommandOutput& output, MazeCommandParam const& param) {
            auto* player = getPlayer(origin);
            if (player == nullptr) {
                output.error("MazeGenerator can only be used by a player.");
                return;
            }
            auto options       = optionsFromPlayer(*player);
            options.mazeSizeX = param.mazeSizeX;
            options.mazeSizeZ = param.mazeSizeZ;
            options.cellSize  = param.cellSize;
            options.wallHeight = param.wallHeight;
            options.wallBlock = param.wallBlock;
            startMaze(origin, output, std::move(options));
        });

    command.overload<MazeAdvancedParam>()
        .required("mazeSizeX")
        .required("mazeSizeZ")
        .required("cellSize")
        .required("wallHeight")
        .required("wallBlock")
        .required("floorBlock")
        .required("topBlock")
        .required("hasExits")
        .required("additionalExits")
        .required("hasRoom")
        .required("roomSizeX")
        .required("roomSizeZ")
        .required("erosion")
        .required("closed")
        .required("hollow")
        .required("blocksPerTick")
        .required("seed")
        .execute([](CommandOrigin const& origin, CommandOutput& output, MazeAdvancedParam const& param) {
            auto* player = getPlayer(origin);
            if (player == nullptr) {
                output.error("MazeGenerator can only be used by a player.");
                return;
            }
            auto options             = optionsFromPlayer(*player);
            options.mazeSizeX        = param.mazeSizeX;
            options.mazeSizeZ        = param.mazeSizeZ;
            options.cellSize         = param.cellSize;
            options.wallHeight       = param.wallHeight;
            options.wallBlock        = param.wallBlock;
            options.floorBlock       = param.floorBlock;
            options.topBlock         = param.topBlock;
            options.hasExits         = param.hasExits;
            options.additionalExits  = param.additionalExits;
            options.hasRoom          = param.hasRoom;
            options.roomSizeX        = param.roomSizeX;
            options.roomSizeZ        = param.roomSizeZ;
            options.erosion          = param.erosion;
            options.closed           = param.closed;
            options.hollow           = param.hollow;
            options.blocksPerTick    = param.blocksPerTick;
            options.seed             = static_cast<uint32_t>(param.seed);
            startMaze(origin, output, std::move(options));
        });

    command.overload<StopParam>().text("stop").execute([](CommandOrigin const&, CommandOutput& output, StopParam const&) {
        MazeGenerator::getInstance().stop();
        output.success("Maze generation stopped.");
    });
}

} // namespace

MazeGenerator& MazeGenerator::getInstance() {
    static MazeGenerator instance;
    return instance;
}

bool MazeGenerator::load() {
    getSelf().getLogger().info("Loading MazeGenerator...");
    return true;
}

bool MazeGenerator::enable() {
    getSelf().getLogger().info("Enabling MazeGenerator...");
    registerCommand();

    auto& eventBus = ll::event::EventBus::getInstance();
    mListeners.emplace_back(eventBus.emplaceListener<ll::event::ServerLevelTickEvent>(
        [](ll::event::ServerLevelTickEvent&) {
            MazeGenerator::getInstance().tick();
        }
    ));
    return true;
}

bool MazeGenerator::disable() {
    getSelf().getLogger().info("Disabling MazeGenerator...");
    auto& eventBus = ll::event::EventBus::getInstance();
    for (auto const& listener : mListeners) {
        if (listener) {
            eventBus.removeListener(listener);
        }
    }
    mListeners.clear();
    mBuilder.reset();
    mSource = nullptr;
    return true;
}

bool MazeGenerator::start(BlockSource& source, MazeOptions options, std::string& error) {
    auto builder = MazeBuilder::create(source, std::move(options), error);
    if (!builder.has_value()) {
        return false;
    }
    mBuilder = std::make_unique<MazeBuilder>(std::move(*builder));
    mSource  = &source;
    return true;
}

void MazeGenerator::stop() {
    mBuilder.reset();
    mSource = nullptr;
}

void MazeGenerator::tick() {
    if (!mBuilder || mSource == nullptr) {
        return;
    }

    mBuilder->tick(*mSource);
    auto const stats = mBuilder->stats();
    if (mBuilder->done()) {
        getSelf().getLogger().info("Maze generation finished: {}/{} blocks", stats.placedBlocks, stats.totalBlocks);
        mBuilder.reset();
        mSource = nullptr;
    }
}

} // namespace maze

LL_REGISTER_MOD(maze::MazeGenerator, maze::MazeGenerator::getInstance());
