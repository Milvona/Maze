# MazeGenerator

MazeGenerator is a LeviLamina native plugin for generating configurable mazes in
Minecraft Bedrock Dedicated Server.

The implementation combines the simple coordinate-driven idea from the
PocketMine MazeGenerator with the production-oriented behavior of larger maze
plugins: deterministic layout generation, exits, optional central rooms, erosion,
closed roofs, hollow walls, custom materials, and tick-budgeted block placement.

## Features

- `/maze` and `/mazegen` commands.
- Random depth-first maze generation with optional seed.
- Configurable maze width, depth, cell size, and wall height.
- Custom wall, floor, and roof block identifiers.
- Optional exits and extra exits.
- Optional central room carving.
- Optional erosion for less rigid corridors.
- Optional closed roofs and hollow wall cells.
- Incremental block placement with a per-tick budget.

## Commands

Run `/maze` as an operator or game director from inside the world. The maze starts
at your feet position.

```text
/maze
/maze <mazeSizeX> <mazeSizeZ> <cellSize> <wallHeight> <wallBlock>
/maze <mazeSizeX> <mazeSizeZ> <cellSize> <wallHeight> <wallBlock> <floorBlock> <topBlock> <hasExits> <additionalExits> <hasRoom> <roomSizeX> <roomSizeZ> <erosion> <closed> <hollow> <blocksPerTick> <seed>
/maze stop
```

Examples:

```text
/maze 51 51 2 4 minecraft:stone
/maze 75 75 3 5 minecraft:deepslate_bricks minecraft:smooth_stone minecraft:stone_brick_slab true 2 true 9 9 0.03 false true 5000 12345
```

## Build

```powershell
xmake f -y -p windows -a x64 -m release
xmake build
```

The packed plugin is generated under `bin/`.

## Notes

Very large mazes can still place millions of blocks. Start with smaller
`blocksPerTick` values on busy servers, then raise the budget after checking TPS.

## License

This project uses the license from
https://github.com/SculkCatalystMC/ProxyPass/blob/main/LICENSE.
