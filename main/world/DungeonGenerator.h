#pragma once
#include "Room.h"
namespace DungeonGenerator {
// Version 1: gleicher Seed ergibt dieselbe 64x48-Karte.
void generate(Room::Map& map, uint32_t seed);
}
