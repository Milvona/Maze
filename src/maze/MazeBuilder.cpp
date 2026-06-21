#include "maze/MazeBuilder.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockChangeContext.h"

namespace maze {
namespace {

int makeOdd(int value, int minValue) {
    value = std::max(value, minValue);
    return value % 2 == 0 ? value + 1 : value;
}

int clampInt(int value, int minValue, int maxValue) { return std::max(minValue, std::min(value, maxValue)); }

double clampDouble(double value, double minValue, double maxValue) { return std::max(minValue, std::min(value, maxValue)); }

uint32_t makeSeed(uint32_t seed) {
    if (seed != 0) {
        return seed;
    }
    return static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
}

} // namespace

std::optional<MazeBuilder> MazeBuilder::create(BlockSource&, MazeOptions options, std::string& error) {
    auto wallBlock = Block::tryGetFromRegistry(HashedString{options.wallBlock}, 0);
    if (!wallBlock) {
        error = "Unknown wall block: " + options.wallBlock;
        return std::nullopt;
    }

    auto floorBlock = Block::tryGetFromRegistry(HashedString{options.floorBlock}, 0);
    if (!floorBlock) {
        error = "Unknown floor block: " + options.floorBlock;
        return std::nullopt;
    }

    auto topBlock = Block::tryGetFromRegistry(HashedString{options.topBlock}, 0);
    if (!topBlock) {
        error = "Unknown top block: " + options.topBlock;
        return std::nullopt;
    }

    MazeBuilder builder{std::move(options), *wallBlock, *floorBlock, *topBlock};
    builder.normalizeOptions();
    builder.generateLayout();
    builder.queuePlacements();
    return builder;
}

MazeBuilder::MazeBuilder(MazeOptions options, Block const& wallBlock, Block const& floorBlock, Block const& topBlock)
: mOptions(std::move(options)),
  mWallBlock(&wallBlock),
  mFloorBlock(&floorBlock),
  mTopBlock(&topBlock),
  mRandom(makeSeed(mOptions.seed)) {}

void MazeBuilder::normalizeOptions() {
    mOptions.mazeSizeX       = makeOdd(mOptions.mazeSizeX, 5);
    mOptions.mazeSizeZ       = makeOdd(mOptions.mazeSizeZ, 5);
    mOptions.cellSize        = clampInt(mOptions.cellSize, 1, 8);
    mOptions.wallHeight      = clampInt(mOptions.wallHeight, 1, 64);
    mOptions.additionalExits = clampInt(mOptions.additionalExits, 0, 32);
    mOptions.roomSizeX       = makeOdd(mOptions.roomSizeX, 3);
    mOptions.roomSizeZ       = makeOdd(mOptions.roomSizeZ, 3);
    mOptions.roomSizeX       = std::min(mOptions.roomSizeX, mOptions.mazeSizeX - 2);
    mOptions.roomSizeZ       = std::min(mOptions.roomSizeZ, mOptions.mazeSizeZ - 2);
    mOptions.erosion         = clampDouble(mOptions.erosion, 0.0, 1.0);
    mOptions.blocksPerTick   = clampInt(mOptions.blocksPerTick, 1, 50000);
}

void MazeBuilder::generateLayout() {
    mGrid.assign(static_cast<std::size_t>(mOptions.mazeSizeX * mOptions.mazeSizeZ), Cell::Wall);

    struct Cursor {
        int x;
        int z;
    };

    std::vector<Cursor> stack;
    stack.push_back({1, 1});
    mGrid[index(1, 1)] = Cell::Path;

    std::array<Cursor, 4> directions{{{2, 0}, {-2, 0}, {0, 2}, {0, -2}}};

    while (!stack.empty()) {
        auto current = stack.back();
        std::shuffle(directions.begin(), directions.end(), mRandom);

        bool moved = false;
        for (auto const& direction : directions) {
            int const nextX = current.x + direction.x;
            int const nextZ = current.z + direction.z;
            if (!inBounds(nextX, nextZ) || mGrid[index(nextX, nextZ)] == Cell::Path) {
                continue;
            }

            mGrid[index(current.x + direction.x / 2, current.z + direction.z / 2)] = Cell::Path;
            mGrid[index(nextX, nextZ)]                                             = Cell::Path;
            stack.push_back({nextX, nextZ});
            moved = true;
            break;
        }

        if (!moved) {
            stack.pop_back();
        }
    }

    if (mOptions.hasRoom) {
        carveRoom();
    }
    if (mOptions.hasExits) {
        carveExits();
    }
    if (mOptions.erosion > 0.0) {
        applyErosion();
    }
}

void MazeBuilder::carveRoom() {
    int const minX = std::max(1, (mOptions.mazeSizeX - mOptions.roomSizeX) / 2);
    int const minZ = std::max(1, (mOptions.mazeSizeZ - mOptions.roomSizeZ) / 2);
    int const maxX = std::min(mOptions.mazeSizeX - 2, minX + mOptions.roomSizeX - 1);
    int const maxZ = std::min(mOptions.mazeSizeZ - 2, minZ + mOptions.roomSizeZ - 1);

    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            mGrid[index(x, z)] = Cell::Path;
        }
    }
}

void MazeBuilder::carveExits() {
    std::vector<BlockPos> exits;
    for (int x = 1; x < mOptions.mazeSizeX - 1; x += 2) {
        if (isPath(x, 1)) exits.emplace_back(x, 0, 0);
        if (isPath(x, mOptions.mazeSizeZ - 2)) exits.emplace_back(x, 0, mOptions.mazeSizeZ - 1);
    }
    for (int z = 1; z < mOptions.mazeSizeZ - 1; z += 2) {
        if (isPath(1, z)) exits.emplace_back(0, 0, z);
        if (isPath(mOptions.mazeSizeX - 2, z)) exits.emplace_back(mOptions.mazeSizeX - 1, 0, z);
    }

    if (exits.empty()) {
        return;
    }

    std::shuffle(exits.begin(), exits.end(), mRandom);
    int const count = std::min<int>(static_cast<int>(exits.size()), 1 + mOptions.additionalExits);
    for (int i = 0; i < count; ++i) {
        mGrid[index(exits[i].x, exits[i].z)] = Cell::Path;
    }
}

void MazeBuilder::applyErosion() {
    std::uniform_real_distribution<double> chance(0.0, 1.0);
    std::vector<Cell> next = mGrid;

    for (int z = 1; z < mOptions.mazeSizeZ - 1; ++z) {
        for (int x = 1; x < mOptions.mazeSizeX - 1; ++x) {
            if (mGrid[index(x, z)] != Cell::Wall || chance(mRandom) > mOptions.erosion) {
                continue;
            }

            int pathNeighbors = 0;
            pathNeighbors += isPath(x + 1, z) ? 1 : 0;
            pathNeighbors += isPath(x - 1, z) ? 1 : 0;
            pathNeighbors += isPath(x, z + 1) ? 1 : 0;
            pathNeighbors += isPath(x, z - 1) ? 1 : 0;
            if (pathNeighbors >= 1) {
                next[index(x, z)] = Cell::Path;
            }
        }
    }

    mGrid = std::move(next);
}

void MazeBuilder::queuePlacements() {
    mPlacements.clear();
    mCursor = 0;

    for (int z = 0; z < mOptions.mazeSizeZ; ++z) {
        for (int x = 0; x < mOptions.mazeSizeX; ++x) {
            queueCell(x, z, mGrid[index(x, z)]);
        }
    }
}

void MazeBuilder::queueCell(int gridX, int gridZ, Cell cell) {
    int const minX = mOptions.originX + gridX * mOptions.cellSize;
    int const minZ = mOptions.originZ + gridZ * mOptions.cellSize;
    int const maxX = minX + mOptions.cellSize - 1;
    int const maxZ = minZ + mOptions.cellSize - 1;
    int const y    = mOptions.originY;

    queueCuboid({minX, y - 1, minZ}, {maxX, y - 1, maxZ}, *mFloorBlock);

    if (cell == Cell::Path) {
        auto air = Block::tryGetFromRegistry(HashedString{"minecraft:air"}, 0);
        if (air) {
            queueCuboid({minX, y, minZ}, {maxX, y + mOptions.wallHeight - 1, maxZ}, *air);
            if (mOptions.closed) {
                queueCuboid({minX, y + mOptions.wallHeight, minZ}, {maxX, y + mOptions.wallHeight, maxZ}, *mTopBlock);
            }
        }
        return;
    }

    queueCuboid(
        {minX, y, minZ},
        {maxX, y + mOptions.wallHeight - 1, maxZ},
        *mWallBlock,
        mOptions.hollow && mOptions.cellSize > 1
    );
    queueCuboid({minX, y + mOptions.wallHeight, minZ}, {maxX, y + mOptions.wallHeight, maxZ}, *mTopBlock);
}

void MazeBuilder::queueCuboid(BlockPos const& min, BlockPos const& max, Block const& block, bool outlineOnly) {
    for (int y = min.y; y <= max.y; ++y) {
        for (int z = min.z; z <= max.z; ++z) {
            for (int x = min.x; x <= max.x; ++x) {
                if (outlineOnly && x != min.x && x != max.x && z != min.z && z != max.z) {
                    continue;
                }
                mPlacements.push_back({{x, y, z}, &block});
            }
        }
    }
}

MazeStats MazeBuilder::stats() const { return {mPlacements.size(), mCursor}; }

bool MazeBuilder::done() const { return mCursor >= mPlacements.size(); }

void MazeBuilder::tick(BlockSource& source) {
    int placed = 0;
    BlockChangeContext context;
    while (mCursor < mPlacements.size() && placed < mOptions.blocksPerTick) {
        auto const& placement = mPlacements[mCursor++];
        source.setBlock(placement.pos, *placement.block, 3, nullptr, context);
        ++placed;
    }
}

int MazeBuilder::index(int gridX, int gridZ) const { return gridZ * mOptions.mazeSizeX + gridX; }

bool MazeBuilder::inBounds(int gridX, int gridZ) const {
    return gridX > 0 && gridZ > 0 && gridX < mOptions.mazeSizeX - 1 && gridZ < mOptions.mazeSizeZ - 1;
}

bool MazeBuilder::isPath(int gridX, int gridZ) const {
    return gridX >= 0 && gridZ >= 0 && gridX < mOptions.mazeSizeX && gridZ < mOptions.mazeSizeZ
        && mGrid[index(gridX, gridZ)] == Cell::Path;
}

} // namespace maze
