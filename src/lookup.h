/*
 * lookup.h - debugging data structures, mostly to allow conversion of enums to their string names
 *   Copyright 2019-2021 tlongstretch
 */

#ifndef SRC_LOOKUP_H_
#define SRC_LOOKUP_H_

#include <bitset>      // used for RoadOptions bools
#include <set>         // for MapPosSet typedef
#include <vector>      // for MapPosVector typedef
#include <utility>     // to satisfy cpplint
#include <map>         // to satisfy cpplint
#include <string>      // to satisfy cpplint
#include "src/building.h"  // used to know Building classconst char*
#include "src/gfx.h"   // for AI overlay, needed to get Color class, maybe find a simpler way?
#include "src/pathfinder.h"  // for tile SearchNode/PSearchNode

//
// NOTE - cpplint says to change const std:string to const char* but when I do so it causesconst char*
//   linker error saying these are being redefined??? makes no sense to me.  Leaving them as std::string
//
const std::string NameBuilding[] = {
        "Building::TypeNone",
        "Building::TypeFisher",
        "Building::TypeLumberjack",
        "Building::TypeBoatbuilder",
        "Building::TypeStonecutter",
        "Building::TypeStoneMine",
        "Building::TypeCoalMine",
        "Building::TypeIronMine",
        "Building::TypeGoldMine",
        "Building::TypeForester",
        "Building::TypeStock",
        "Building::TypeHut",
        "Building::TypeFarm",
        "Building::TypeButcher",
        "Building::TypePigFarm",
        "Building::TypeMill",
        "Building::TypeBaker",
        "Building::TypeSawmill",
        "Building::TypeSteelSmelter",
        "Building::TypeToolMaker",
        "Building::TypeWeaponSmith",
        "Building::TypeTower",
        "Building::TypeFortress",
        "Building::TypeGoldSmelter",
        "Building::TypeCastle"
};

const std::string NameTerrain[]{
    "Map::TerrainWater0",
    "Map::TerrainWater1",
    "Map::TerrainWater2",
    "Map::TerrainWater3",
    "Map::TerrainGrass0",
    "Map::TerrainGrass1",
    "Map::TerrainGrass2",
    "Map::TerrainGrass3",
    "Map::TerrainDesert0",
    "Map::TerrainDesert1",
    "Map::TerrainDesert2",
    "Map::TerrainTundra0",
    "Map::TerrainTundra1",
    "Map::TerrainTundra2",
    "Map::TerrainSnow0",
    "Map::TerrainSnow1"
    //"Map::TerrainShroud", // new, index 16, map_ground sprite 33, for option_FogOfWar
};

const std::string NameObject[]{
"Map::ObjectNone",
"Map::ObjectFlag",
"Map::ObjectSmallBuilding",
"Map::ObjectLargeBuilding",
"Map::ObjectCastle",
 "Nothing",
 "Nothing",
 "Nothing",
"Map::ObjectTree0",
"Map::ObjectTree1",
"Map::ObjectTree2",
"Map::ObjectTree3",
"Map::ObjectTree4",
"Map::ObjectTree5",
"Map::ObjectTree6",
"Map::ObjectTree7",
"Map::ObjectPine0",
"Map::ObjectPine1",
"Map::ObjectPine2",
"Map::ObjectPine3",
"Map::ObjectPine4",
"Map::ObjectPine5",
"Map::ObjectPine6",
"Map::ObjectPine7",
"Map::ObjectPalm0",
"Map::ObjectPalm1",
"Map::ObjectPalm2",
"Map::ObjectPalm3",
"Map::ObjectWaterTree0",
"Map::ObjectWaterTree1",
"Map::ObjectWaterTree2",
"Map::ObjectWaterTree3",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
 "Nothing",
"Map::ObjectStone0",
"Map::ObjectStone1",
"Map::ObjectStone2",
"Map::ObjectStone3",
"Map::ObjectStone4",
"Map::ObjectStone5",
"Map::ObjectStone6",
"Map::ObjectStone7",
"Map::ObjectSandstone0",
"Map::ObjectSandstone1",
"Map::ObjectCross",
"Map::ObjectStub",
"Map::ObjectStone",
"Map::ObjectSandstone3",
"Map::ObjectCadaver0",
"Map::ObjectCadaver1",
"Map::ObjectWaterStone0",
"Map::ObjectWaterStone1",
"Map::ObjectCactus0",
"Map::ObjectCactus1",
"Map::ObjectDeadTree",
"Map::ObjectFelledPine0",
"Map::ObjectFelledPine1",
"Map::ObjectFelledPine2",
"Map::ObjectFelledPine3",
"Map::ObjectFelledPine4",
"Map::ObjectFelledTree0",
"Map::ObjectFelledTree1",
"Map::ObjectFelledTree2",
"Map::ObjectFelledTree3",
"Map::ObjectFelledTree4",
"Map::ObjectNewPine",
"Map::ObjectNewTree",
"Map::ObjectSeeds0",
"Map::ObjectSeeds1",
"Map::ObjectSeeds2",
"Map::ObjectSeeds3",
"Map::ObjectSeeds4",
"Map::ObjectSeeds5",
"Map::ObjectFieldExpired",
"Map::ObjectSignLargeGold",
"Map::ObjectSignSmallGold",
"Map::ObjectSignLargeIron",
"Map::ObjectSignSmallIron",
"Map::ObjectSignLargeCoal",
"Map::ObjectSignSmallCoal",
"Map::ObjectSignLargeStone",
"Map::ObjectSignSmallStone",
"Map::ObjectSignEmpty",
"Map::ObjectField0",
"Map::ObjectField1",
"Map::ObjectField2",
"Map::ObjectField3",
"Map::ObjectField4",
"Map::ObjectField5",
"Map::Object127",
};

const std::string NameSerf[]{
    "Serf::TypeTransporter",
    "Serf::TypeSailor",
    "Serf::TypeDigger",
    "Serf::TypeBuilder",
    "Serf::TypeTransporterInventory",
    "Serf::TypeLumberjack",
    "Serf::TypeSawmiller",
    "Serf::TypeStonecutter",
    "Serf::TypeForester",
    "Serf::TypeMiner",
    "Serf::TypeSmelter",
    "Serf::TypeFisher",
    "Serf::TypePigFarmer",
    "Serf::TypeButcher",
    "Serf::TypeFarmer",
    "Serf::TypeMiller",
    "Serf::TypeBaker",
    "Serf::TypeBoatBuilder",
    "Serf::TypeToolmaker",
    "Serf::TypeWeaponSmith",
    "Serf::TypeGeologist",
    "Serf::TypeGeneric",
    "Serf::TypeKnight0",
    "Serf::TypeKnight1",
    "Serf::TypeKnight2",
    "Serf::TypeKnight3",
    "Serf::TypeKnight4",
    "Serf::TypeDead"
};

const std::string NameTool[]{
        "shovel",
        "hammer",
        "rod",
        "cleaver",
        "scythe",
        "axe",
        "saw",
        "pick",
        "pincer"
};

const std::string NameResource[] = {
  //  "Resource::TypeNone" = -1   // NOTE because this is an array of strings, and not a real enum, you cannot have negative values!  
      "Resource::TypeFish",
      "Resource::TypePig",
      "Resource::TypeMeat",
      "Resource::TypeWheat",
      "Resource::TypeFlour",
      "Resource::TypeBread",
      "Resource::TypeLumber",
      "Resource::TypePlank",
      "Resource::TypeBoat",
      "Resource::TypeStone",
      "Resource::TypeIronOre",
      "Resource::TypeSteel",
      "Resource::TypeCoal",
      "Resource::TypeGoldOre",
      "Resource::TypeGoldBar",
      "Resource::TypeShovel",
      "Resource::TypeHammer",
      "Resource::TypeRod",
      "Resource::TypeCleaver",
      "Resource::TypeScythe",
      "Resource::TypeAxe",
      "Resource::TypeSaw",
      "Resource::TypePick",
      "Resource::TypePincer",
      "Resource::TypeSword",
      "Resource::TypeShield",
      "Resource::GroupFood",
      "Resource::TypeSerf"
};



// Map directions
//
//    A ______ B
//     /\    /
//    /  \  /
// C /____\/ D
//
// Six standard directions:
// RIGHT: A to B
// DOWN_RIGHT: A to D
// DOWN: A to C
// LEFT: D to C
// UP_LEFT: D to A
// UP: D to B
//
// Non-standard directions:
// UP_RIGHT: C to B
// DOWN_LEFT: B to C
//
//typedef enum Direction {
//      DirectionNone = -1,
//
//      DirectionRight = 0,
//      DirectionDownRight,
//      DirectionDown,
//      DirectionLeft,
//      DirectionUpLeft,
//      DirectionUp,
//} Direction;


const std::string NameDirection[]{
  // NOTE THAT THIS WILL RESULT IN ARRAY INDEX OUT OF BOUNDS EXCEPTION
  //  IF DirectionNone  -1 is given as it cannot be defined as an array element!
  "East / Right",            // 0
  "SouthEast / DownRight",   // 1
  "SouthWest / Down",        // 2
  "West / Left",             // 3
  "NorthWest / UpLeft",      // 4
  "NorthEast / Up"           // 5
};


const std::string NamePlayerFace[]{
  "InvalidFace0",
  "Lady Amelie",
  "Kumpy Onefinger",
  "Balduin, a former monk",
  "Frollin",
  "Kallina",
  "Rasparuk the Druid",
  "Count Aldaba",
  "King Ralph VII",
  "Homen Doublehorn",
  "Solluk the Joker",
  "Last Enemy",
  "You",
  "Friend"
};

// road_options Road::Options Road::Option bitset
const std::string NameRoadOption[] = {
  "Direct",
  "SplitRoads",
  "PenalizeNewLength",
  "AvoidCastleArea",    // this currently does nothing! needs coding
  "PenalizeCastleFlag",
  "Improve",
  "ReducedNewLengthPenalty",
  "AllowWaterRoad",
  "HoldBuildingPos",
  "MostlyStraight",
  "PlotOnlyNoBuild",
  "ReconnectNetwork",
};

const std::string NameMinerals[] = {
  "MineralsNone",   // fish???
  "MineralsGold",
  "MineralsIron",
  "MineralsCoal",
  "MineralsStone",
};





const std::string building_sizes[25] = {
"None",                   // BUILDING_NONE
"SmallCivilian",          // BUILDING_FISHER
"SmallCivilian",          // BUILDING_LUMBERJACK
"SmallCivilian",          // BUILDING_BOATBUILDER
"SmallCivilian",          // BUILDING_STONECUTTER
"Mine",                   // BUILDING_STONEMINE
"Mine",                   // BUILDING_COALMINE
"Mine",                   // BUILDING_IRONMINE
"Mine",                   // BUILDING_GOLDMINE
"SmallCivilian",          // BUILDING_FORESTER
"LargeCivilian",          // BUILDING_STOCK
"SmallMilitary",          // BUILDING_HUT
"LargeCivilian",          // BUILDING_FARM
"LargeCivilian",          // BUILDING_BUTCHER
"LargeCivilian",          // BUILDING_PIGFARM
"SmallCivilian",          // BUILDING_MILL
"LargeCivilian",          // BUILDING_BAKER
"LargeCivilian",          // BUILDING_SAWMILL
"LargeCivilian",          // BUILDING_STEELSMELTER
"LargeCivilian",          // BUILDING_TOOLMAKER
"LargeCivilian",          // BUILDING_WEAPONSMITH
"LargeMilitary",          // BUILDING_TOWER
"LargeMilitary",          // BUILDING_FORTRESS
"LargeCivilian",          // BUILDING_GOLDSMELTER
"Castle",                 // BUILDING_CASTLE
};


// copied from inventory.cc, not sure how to get it directly from there without copying
const Resource::Type tools_needed[] = {
  //none                                   none                                   // TypeNone = -1
    Resource::TypeNone,    Resource::TypeNone,    // SERF_TRANSPORTER = 0,
    Resource::TypeBoat,    Resource::TypeNone,    // SERF_SAILOR,
    Resource::TypeShovel,  Resource::TypeNone,    // SERF_DIGGER,
    Resource::TypeHammer,  Resource::TypeNone,    // SERF_BUILDER,
    Resource::TypeNone,    Resource::TypeNone,    // SERF_TRANSPORTER_INVENTORY,
    Resource::TypeAxe,     Resource::TypeNone,    // SERF_LUMBERJACK,
    Resource::TypeSaw,     Resource::TypeNone,    // TypeSawmiller,
    Resource::TypePick,    Resource::TypeNone,    // TypeStonecutter,
    Resource::TypeNone,    Resource::TypeNone,    // TypeForester,
    Resource::TypePick,    Resource::TypeNone,    // TypeMiner,
    Resource::TypeNone,    Resource::TypeNone,    // TypeSmelter,
    Resource::TypeRod,     Resource::TypeNone,    // TypeFisher,
    Resource::TypeNone,    Resource::TypeNone,    // TypePigFarmer,
    Resource::TypeCleaver, Resource::TypeNone,    // TypeButcher,
    Resource::TypeScythe,  Resource::TypeNone,    // TypeFarmer,
    Resource::TypeNone,    Resource::TypeNone,    // TypeMiller,
    Resource::TypeNone,    Resource::TypeNone,    // TypeBaker,
    Resource::TypeHammer,  Resource::TypeNone,    // TypeBoatBuilder,
    Resource::TypeHammer,  Resource::TypeSaw,     // TypeToolmaker,
    Resource::TypeHammer,  Resource::TypePincer,  // TypeWeaponSmith,
    Resource::TypeHammer,  Resource::TypeNone,    // TypeGeologist,
    Resource::TypeNone,    Resource::TypeNone,    // TypeGeneric,
    Resource::TypeSword,   Resource::TypeShield,  // TypeKnight0,
    Resource::TypeNone,    Resource::TypeNone,    // TypeKnight1,
    Resource::TypeNone,    Resource::TypeNone,    // TypeKnight2,
    Resource::TypeNone,    Resource::TypeNone,    // TypeKnight3,
    Resource::TypeNone,    Resource::TypeNone,    // TypeKnight4,
    Resource::TypeNone,    Resource::TypeNone,    // TypeDead
};



// knight staffing levels by building type and threat level
    //        Hut Tower     Garrison
    //      Full    3       6       12
    //      Good    2       4       9
    //      Medium  2       3       6
    //      Weak    1       2       3
    //      Minimum 1       1       1

const unsigned int slots_hut[5] = { 1,1,2,2,3 };
const unsigned int slots_tower[5] = { 1,2,3,4,6 };
const unsigned int slots_garrison[5] = { 1,3,6,9,12 };


typedef std::set<std::pair<MapPos, unsigned int>> MapPosSet;
typedef std::vector<MapPos> MapPosVector;
typedef std::map<MapPos, std::list<PSearchNode>> ClosedNodesByStartPos;
typedef std::map<MapPos, std::vector<PSearchNode>> OpenNodesByStartPos;

// key pos/dir -> val unordered(?) list of flags, assume starting at the Inventory
typedef std::map<std::pair<MapPos, Direction>, MapPosVector> FlagDirToFlagVectorMap;
// key pos/dir -> val ordered list of flag-dir pairs, assume starting at the Inventory
//  this could be a Set instead of Vector to ensure uniqueness, but doesn't seem necessary
typedef std::map<std::pair<MapPos, Direction>, std::vector<std::pair<MapPos,Direction>>> FlagDirToFlagDirVectorMap;
typedef std::vector<std::pair<MapPos, Direction>> MapPosDirVector;

// for AI overlay debugging markers
typedef const std::map<std::string, Color> Colors;
const Colors colors = {
{ "black", Color(0x00,0x00,0x00)} ,  { "white", Color(0xFF,0xFF,0xFF)} ,  { "lt_brown", Color(0xD2, 0xB4, 0x8C)} ,  { "lt_red", Color(0xFF,0x99,0x99)} ,
{ "lt_orange", Color(0xFF,0xCC,0x99)} ,  { "lt_yellow", Color(0xFF,0xFF,0x99)} ,  { "lt_lime", Color(0xCC,0xFF,0x99)} ,  { "lt_green", Color(0x99,0xFF,0x99)} ,
{ "lt_seafoam", Color(0x99,0xFF,0xCC)} ,  { "lt_cyan", Color(0x99,0xFF,0xFF)} ,  { "lt_lavender", Color(0x99,0xCC,0xFF)} ,  { "lt_blue", Color(0x99,0x99,0xFF)} ,
{ "lt_purple", Color(0xCC,0x99,0xFF)} ,  { "lt_magenta", Color(0xFF,0x99,0xFF)} ,  { "lt_coral", Color(0xFF,0x99,0xCC)} ,  { "lt_gray", Color(0xE0,0xE0,0xE0)} ,
{ "brown", Color(0xA0, 0x52, 0x2D)} ,  { "red", Color(0xFF,0x00,0x00)} ,  { "orange", Color(0xFF,0x80,0x00)} ,  { "yellow", Color(0xFF,0xFF,0x00)} ,
{ "lime", Color(0x80,0xFF,0x00)} ,  { "green", Color(0x00,0xFF,0x00)} ,  { "seafoam", Color(0x00,0xFF,0x80)} ,  { "cyan", Color(0x00,0xFF,0xFF)} ,
{ "lavender", Color(0x00,0x80,0xFF)} ,  { "blue", Color(0x00,0x00,0xFF)} ,  { "purple", Color(0x7F,0x00,0xFF)} ,  { "magenta", Color(0xFF,0x00,0xFF)} ,
{ "coral", Color(0xFF,0x00,0x7F)} ,  { "gray", Color(0x80,0x80,0x80)} ,  { "dk_brown", Color(0x8B, 0x45, 0x13)} ,  { "dk_red", Color(0xCC,0x00,0x00)} ,
{ "dk_orange", Color(0xCC,0x66,0x00)} ,  { "dk_yellow", Color(0xCC,0xCC,0x00)} ,  { "dk_lime", Color(0x66,0xCC,0x00)} ,  { "dk_green", Color(0x00,0xCC,0x00)} ,
{ "dk_seafoam", Color(0x00,0xCC,0x66)} ,  { "dk_cyan", Color(0x00,0xCC,0xCC)} ,  { "dk_lavender", Color(0x00,0x66,0xCC)} ,  { "dk_blue", Color(0x00,0x00,0xCC)} ,
{ "dk_purple", Color(0x66,0x00,0xCC)} ,  { "dk_magenta", Color(0xCC,0x00,0xCC)} ,  { "dk_coral", Color(0xCC,0x00,0x66)} ,  { "dk_gray", Color(0x60,0x60,0x60)} ,
};
typedef std::pair<MapPos, std::string> ColorDot;
typedef std::map<MapPos, std::string> ColorDotMap;



// convert distance-from-center to MapPos offset for add_spirally
const int _spiral_dist[25] = { 1, 7, 19, 37, 61, 91, 127, 169, 217, 271, 331, 397,
  469, 547, 631, 721, 817, 919, 1027, 1141, 1261, 1387, 1519, 1657, 1801 };
//  one spiral row count equals (6*rows from center) + (previous row's value)

// ***** I don't think any of the specialists_reserve or specialists_max works at all *********
//
// this stuff isn't really for lookups, find a better place to put it
//
// if any idle+potential is below this number, build ToolMaker
//       if a toolmaker already exists, Housekeeping job will try to
//        build tools for any specialist count < 1

//
//  IMPROVEMENT NEEDED - distinguish between RESERVE professionals and REALM professionals, including ones placed in buildings
//        RESERVE professionals kept in castle to be deployed quickly
//                once REALM professionals is met, don't need any more RESERVES (such as 3x lumberjacks, 1x miller, 1x sawmiller, etc etc.)
//

// NOTE - min/max/reserve specialists logic being replaced with simple "ensure > 0" rule now that multiple economies working
//  this may waste a few Steel & Wood, but should be acceptable

const unsigned int specialists_reserve[] = {
  //0             // TypeNone = -1
    0,    // TypeTransporter = 0,
    0,    // TypeSailor,          // AI doesn't use boats yet
    0,    // TypeDigger,      // always start with one and only need one
    1,    // TypeBuilder,
    0,    // TypeTransporterInventory,
    1,    // TypeLumberjack,
    0,    // TypeSawmiller,   // always start with one and only need one
    0,    // TypeStonecutter, // always start with one and only need one
    0,    // TypeForester,        // doesn't need tools
    1,    // TypeMiner,
    0,    // TypeSmelter,         // doesn't need tools
    1,    // TypeFisher,          // not essential, but nice to have
    0,    // TypePigFarmer,       // AI doesn't use pig farms yet
    0,    // TypeButcher,         // AI doesn't use butchers yet
    1,    // TypeFarmer,      // don't start with one on really low resource starts
    0,    // TypeMiller,          // doesn't need tools
    0,    // TypeBaker,           // doesn't need tools
    0,    // TypeBoatBuilder, // AI doesn't use boats yet, think this uses Hammers anyway
    0,    // TypeToolmaker,       // doesn't need tools
    1,    // TypeWeaponSmith,
    1,    // TypeGeologist,
    0,    // TypeGeneric,
    0,    // TypeKnight0,
    0,    // TypeKnight1,
    0,    // TypeKnight2,
    0,    // TypeKnight3,
    0,    // TypeKnight4,
    0,    // TypeDead
};

/*
//
//  these max values are only appropriate PER INVENTORY, once warehouses are built
//   these should be evaluated per warehouse... or at least multiplied * number of warehouses
//     this is probably a bug, I haven't looked into it yet
//
const unsigned int specialists_max[] = {
  //0     // TypeNone = -1
    0,    // TypeTransporter
    0,    // TypeSailor,          // AI doesn't use boats yet, uses boat
    2,    // TypeDigger,          // always start with one and only need one, uses shovel
    5,    // TypeBuilder,         // uses hammer
    0,    // TypeTransporterInventory,
    3,    // TypeLumberjack,      // always start with one, need up to three, uses axe
    1,    // TypeSawmiller,       // always start with one and only need one, uses saw??
    1,    // TypeStonecutter,     // always start with one and only need one, uses pickaxe
    0,    // TypeForester,        // doesn't need tools
    3,    // TypeMiner,           // uses pickaxe
    0,    // TypeSmelter,         // doesn't need tools
    3,    // TypeFisher,          // not essential, but nice to have, uses rod
    0,    // TypePigFarmer,       // AI doesn't use pig farms yet
    0,    // TypeButcher,         // AI doesn't use butchers yet, uses cleaver
    3,    // TypeFarmer,          // we don't start with one on really low resource starts, doesn't need tools
    0,    // TypeMiller,          // doesn't need tools
    0,    // TypeBaker,           // doesn't need tools
    0,    // TypeBoatBuilder,     // AI doesn't use boats yet, think this uses Hammers anyway
    0,    // TypeToolmaker,       // doesn't need tools
    1,    // TypeWeaponSmith,     // uses hammer and pincer/pliars
    4,    // TypeGeologist,       // uses hammer
    0,    // TypeGeneric,
    0,    // TypeKnight0,         // uses sword and shield
    0,    // TypeKnight1,         // promoted from Knight0
    0,    // TypeKnight2,         // promoted from Knight1
    0,    // TypeKnight3,         // promoted from Knight2
    0,    // TypeKnight4,         // promoted from Knight3
    0,    // TypeDead
};
*/

// when building roads, try to connect a road to each type
//  if none or one type not-None, will build one road
//  if both types non-None, will try to build road to each
const Building::Type BuildingAffinity[25][2] = {
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeNone",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeFisher",
    { Building::TypeSawmill, Building::TypeNone},   //              "Building::TypeLumberjack",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeBoatbuilder",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeStonecutter",
    { Building::TypeBaker, Building::TypeNone},     //              "Building::TypeStoneMine",
    { Building::TypeWeaponSmith, Building::TypeSteelSmelter}, //    "Building::TypeCoalMine",
    { Building::TypeBaker, Building::TypeSteelSmelter},     //      "Building::TypeIronMine",
    { Building::TypeBaker, Building::TypeGoldSmelter},      //      "Building::TypeGoldMine",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeForester",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeStock",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeHut",
    { Building::TypeMill, Building::TypeNone},      //              "Building::TypeFarm",
    { Building::TypeGoldMine, Building::TypeIronMine},      //      "Building::TypeButcher",
    { Building::TypeButcher, Building::TypeCoalMine},       //      "Building::TypePigFarm",
    { Building::TypeFarm, Building::TypeNone},      //              "Building::TypeMill",
    { Building::TypeMill, Building::TypeCoalMine},          //      "Building::TypeBaker",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeSawmill",
    { Building::TypeIronMine, Building::TypeCoalMine},      //      "Building::TypeSteelSmelter",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeToolMaker",
    { Building::TypeSteelSmelter, Building::TypeCoalMine},  //      "Building::TypeWeaponSmith",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeTower",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeFortress",
    { Building::TypeGoldMine, Building::TypeCoalMine},      //      "Building::TypeGoldSmelter",
    { Building::TypeNone, Building::TypeNone},      //              "Building::TypeCastle"
};
/*defaults:
  road_options.reset(RoadOption::Direct);
  road_options.set(RoadOption::SplitRoads);
  road_options.set(RoadOption::PenalizeNewLength);
  road_options.set(RoadOption::PenalizeCastleFlag);
  road_options.reset(RoadOption::AvoidCastleArea);    // this currently does nothing! needs coding
  road_options.reset(RoadOption::Improve);
  road_options.reset(RoadOption::ReducedNewLengthPenalty);
  road_options.reset(RoadOption::AllowWaterRoad);
  road_options.reset(RoadOption::HoldBuildingPos);
  road_options.reset(RoadOption::MostlyStraight);
  road_options.reset(RoadOption::PlotOnlyNoBuild);
  road_options.reset(RoadOption::ReconnectNetwork);
  */

typedef enum RoadOption {
  Direct = 0,         // connect directly to target building's flag, [not?] any potential routes via other flags, no splitting roads, ignores Improve flag (will build regardless)
  SplitRoads,         // allow creating new flags (splitting roads) when optimal
  PenalizeNewLength,  // disfavor creating new road length, prefer connecting to existing paths when optimal
  AvoidCastleArea,      // this currently does nothing! needs coding      // disfavor any pos surrounding the castle, to avoid congesting that area
  PenalizeCastleFlag, // disfavor any flag-path that includes the castle flag, for resources that route directly to consumers
  Improve,            // allow creating new roads even if start_pos already has any existing path (otherwise disallow it)
  ReducedNewLengthPenalty, // reduced penalty, experimental, probably should just change overall new_length_penalty to penalize longer roads less
  AllowWaterRoad,     // allow creating water roads, this is on (OFF NOW) by default but if a water road is about to be built the build_best_road function will
                      //   make a recursive call to itself with AllowWater disabled to ensure that a land route is also available, to avoid issue where
                      //   serfs cannot reach the construction site/building because serfs cannot travel in boats (which is dumb)
                      //  UPDATE jan02 2022, turning off AllowWater roads, they don't get good use by AI and are buggy right now
  HoldBuildingPos,    // when plotting a road, do not allow the road to pass through the pos UpLeft from the dest, so it does not prevent a building there
  MostlyStraight,     // reduce the amount of tolerated road convulation before rejecting a solution.  Effect is to reduce the max_convolution ratio
  PlotOnlyNoBuild,    // do not actually build the road, but return a Road object of the best solution found
  ReconnectNetwork    // for trying to reconnect a broken road network, exclude and flags connected to same-network from valid end_flags for pathfinding.  Must connect to ANOTHER network that has a path
} RoadOption;
typedef std::bitset<12> RoadOptions;
typedef std::vector<Road> Roads;


// I was originally setting -1 here to be the worst-possible score, but because other lengths/penalties are ADDED to this score, it wraps back around
//     to be a very-low/good score again!   Instead, setting to HALF of -1 (4294967295) ... which is 2147483647.5 or 2147483648
// GAH, this STILL doesn't work safely, because scores get ADDED and very commonly bad_score + bad_score overflows it
//   changing to a much lower number that is still obviously wrong
//const unsigned int bad_score = 2147483648;
const unsigned int bad_score = 123123123;


//attempted to create a 'caret' search shape instead of 'spiral' ... never completed
//   RATHER THAN DEALING WITH THESE SHAPES IT MIGHT BE SIMPLER TO SIMPLY DO 3 SMALL HEXAGONS IN AN '^' SHAPE!
//  no... it isn't really... wait maybe.. you could draw a single line hexagon to act as centers, and have each corner do one "line" of it
//   you could use existing get_corners code also to get the start of each line
//   and instead of looping over each corner, loop over each direction?
//  or just retrace half your steps in opposite way before starting each corner??
//  yeah... say you start at NE/U corner, just draw a straight line SE/DR until next corner found, then draw SW/D...


/*
  double desert_frequency;

  double trees_both1;
  double trees_deciduous;
  double trees_pine;
  double trees_both2;
  double stonepile_dense;
  double stonepile_sparse;
  double junk_grass_trees_dead;
  double junk_grass_sandstone;
  double junk_water_trees;
  double junk_grass_stub_trees;
  double junk_grass_small_boulders;
  double junk_desert_cadavers;
  double junk_desert_cacti;
  double junk_water_boulders;
  double junk_desert_palm_trees;*/

typedef enum CustomMapGeneratorOption {
  Trees = 0,
  StonepileDense,
  StonepileSparse,
  Fish,
  MountainGold,
  MountainIron,
  MountainCoal,
  MountainStone,
  DesertFrequency,
  LakesWaterLevel,
  JunkGrassDeadTrees,
  JunkGrassSandStone,
  JunkWaterSubmergedTrees,
  JunkGrassStubTrees,
  JunkGrassSmallBoulders,
  JunkDesertAnimalCadavers,
  JunkDesertCacti,
  JunkWaterSubmergedBoulders,
  JunkDesertPalmTrees,
} CustomMapGeneratorOption;

typedef struct CustomMapGeneratorOptions { double opt[23]; } CustomMapGeneratorOptions;

typedef enum CompletionLevel {
  Unfinished = 0,
  Connected = 1,
  Completed = 2,
} CompletionLevel;

const std::string NameCompletionLevel[] = {
  "Unfinished",
  "Connected",
  "Completed",
};

typedef enum DistType {
  FlagOnly = 0,
  StraightLineOnly = 1,
  FlagAndStraightLine = 2,
} DistType;

const std::string NameDistType[] = {
  "FlagOnly",
  "StraightLineOnly",
  "FlagAndStraightLine",
};


// copied from game-init.h so game.cc can use it?
typedef enum GameType {
  GameCustom = 0,
  GameMission = 1,
  GameLoad = 2
} GameType;

typedef enum DataSourceType {
  Amiga = 0,
  DOS = 1,
  Custom = 2
} DataSourceType;

const std::string NameDataSourceType[] = {
  "Amiga",
  "DOS",
  "Custom",
};

const std::string NameSeason[] = {
  "SeasonSpring",
  "SeasonSummer",
  "SeasonFall",
  "SeasonWinter",
};

/* moving this to game_options.h because of multiple declaration issues and I don't understand globals
// the Custom data map_objects offset for tree sprites
int season_offset[4] = {
    0, // Summer
  400, // Fall
  300, // Winter
  200, // Spring
};
*/


#endif  // SRC_LOOKUP_H_
