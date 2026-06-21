#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"

namespace maze {

struct MazeOptions {
    int         originX{0};
    int         originY{64};
    int         originZ{0};
    int         mazeSizeX{31};
    int         mazeSizeZ{31};
    int         cellSize{2};
    int         wallHeight{3};
    bool        hasExits{true};
    int         additionalExits{0};
    bool        hasRoom{false};
    int         roomSizeX{5};
    int         roomSizeZ{5};
    double      erosion{0.0};
    bool        closed{false};
    bool        hollow{false};
    std::string wallBlock{"minecraft:stone"};
    std::string floorBlock{"minecraft:smooth_stone"};
    std::string topBlock{"minecraft:stone_brick_slab"};
    int         blocksPerTick{4096};
    uint32_t    seed{0};
};

struct MazeStats {
    std::size_t totalBlocks{0};
    std::size_t placedBlocks{0};
};

class MazeBuilder {

public:
    static std::optional<MazeBuilder> create(BlockSource& source, MazeOptions options, std::string& error);

    [[nodiscard]] MazeStats stats() const;
    [[nodiscard]] bool      done() const;

    void tick(BlockSource& source);

private:
    enum class Cell : uint8_t { Wall, Path };

    struct Placement {
        BlockPos      pos;
        Block const*  block;
    };

    MazeBuilder(MazeOptions options, Block const& wallBlock, Block const& floorBlock, Block const& topBlock);

    void normalizeOptions();
    void generateLayout();
    void carveRoom();
    void carveExits();
    void applyErosion();
    void queuePlacements();
    void queueCell(int gridX, int gridZ, Cell cell);
    void queueCuboid(BlockPos const& min, BlockPos const& max, Block const& block, bool outlineOnly = false);

    [[nodiscard]] int  index(int gridX, int gridZ) const;
    [[nodiscard]] bool inBounds(int gridX, int gridZ) const;
    [[nodiscard]] bool isPath(int gridX, int gridZ) const;

    MazeOptions            mOptions;
    Block const*           mWallBlock;
    Block const*           mFloorBlock;
    Block const*           mTopBlock;
    std::vector<Cell>      mGrid;
    std::vector<Placement> mPlacements;
    std::size_t            mCursor{0};
    std::mt19937           mRandom;
};

} // namespace maze
