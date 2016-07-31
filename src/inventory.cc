/*
 * inventory.cc - Resources related implementations.
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "src/inventory.h"

#include <cassert>
#include <algorithm>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_CSTDINT
# include <cstdint>
#endif

#include "src/savegame.h"
#include "src/flag.h"
#include "src/game.h"
#include "src/serf.h"

Inventory::Inventory(Game *game, unsigned int index)
  : GameObject(game, index) {
  owner = 0;
  res_dir = 0;
  flag = 0;
  building = 0;
  for (int i = 0; i < 2; i++) {
    out_queue[i].type = Resource::TypeNone;
    out_queue[i].dest = 0;
  }
  serfs_out = 0;
  generic_count = 0;
}

void
Inventory::push_resource(Resource::Type resource) {
  resources[resource] += (resources[resource] < 50000) ? 1 : 0;
}

void
Inventory::get_resource_from_queue(Resource::Type *res, int *dest) {
  *res = out_queue[0].type;
  *dest = out_queue[0].dest;

  out_queue[0].type = out_queue[1].type;
  out_queue[0].dest = out_queue[1].dest;

  out_queue[1].type = Resource::TypeNone;
  out_queue[1].dest = 0;
}

void
Inventory::add_to_queue(Resource::Type type, unsigned int dest) {
  if (type == Resource::GroupFood) {
    /* Select the food resource with highest amount available */
    if (resources[Resource::TypeMeat] > resources[Resource::TypeBread]) {
      if (resources[Resource::TypeMeat] > resources[Resource::TypeFish]) {
        type = Resource::TypeMeat;
      } else {
        type = Resource::TypeFish;
      }
    } else if (resources[Resource::TypeBread] > resources[Resource::TypeFish]) {
      type = Resource::TypeBread;
    } else {
      type = Resource::TypeFish;
    }
  }

  assert(resources[type] != 0);

  resources[type] -= 1;
  if (out_queue[0].type == Resource::TypeNone) {
    out_queue[0].type = type;
    out_queue[0].dest = dest;
  } else {
    out_queue[1].type = type;
    out_queue[1].dest = dest;
  }
}

void
Inventory::reset_queue_for_dest(Flag *flag) {
  if (out_queue[1].type != Resource::TypeNone &&
      out_queue[1].dest == flag->get_index()) {
    push_resource(out_queue[1].type);
    out_queue[1].type = Resource::TypeNone;
  }
  if (out_queue[0].type != Resource::TypeNone &&
      out_queue[0].dest == flag->get_index()) {
    push_resource(out_queue[0].type);
    out_queue[0].type = out_queue[1].type;
    out_queue[0].dest = out_queue[1].dest;
    out_queue[1].type = Resource::TypeNone;
  }
}

void
Inventory::lose_queue() {
  for (int i = 0; i < 2 && out_queue[i].type != Resource::TypeNone; i++) {
    Resource::Type res = out_queue[i].type;
    int dest = out_queue[i].dest;

    game->cancel_transported_resource(res, dest);
    game->lose_resource(res);
  }
}

void
Inventory::apply_supplies_preset(size_t supplies) {
  const int supplies_template_0[] = {  0,  0,  0,  0,  0,  0,  0,   7,  0,
                                       2,  0,   0,  0,  0,  0,  1,  6,  1,
                                       0,  0,  1,  2,  3,  0,  10,  10 };
  const int supplies_template_1[] = {  2,  1,  1,  3,  2,  1,  0,  25,  1,
                                       8,  4,   3,   8,  2, 1,  3, 12,  2,
                                       1,  1,  2,  3,  4,  1,  30,  30 };
  const int supplies_template_2[] = {  3,  2,  2, 10,  3,  1,  0,  40,  2,
                                      20, 12,   8,  20,  4, 2,  5, 20,  3,
                                       1,  2,  3,  4,  6,  2,  60,  60 };
  const int supplies_template_3[] = {  8,  4,  6, 20,  7,  5,  3,  80,  5,
                                      40, 20,  40,  50,  8, 4, 10, 30,  5,
                                       2,  4,  6,  6, 12,  4, 100, 100 };
  const int supplies_template_4[] = { 30, 10, 30, 50, 10, 30, 10, 200, 10,
                                     100, 30, 150, 100, 10, 5, 20, 50, 10,
                                       5, 10, 20, 20, 50, 10, 200, 200 };

  const int *template_1 = NULL;
  const int *template_2 = NULL;
  if (supplies < 10) {
    template_1 = supplies_template_0;
    template_2 = supplies_template_1;
  } else if (supplies < 20) {
    template_1 = supplies_template_1;
    template_2 = supplies_template_2;
    supplies -= 10;
  } else if (supplies < 30) {
    template_1 = supplies_template_2;
    template_2 = supplies_template_3;
    supplies -= 20;
  } else if (supplies < 40) {
    template_1 = supplies_template_3;
    template_2 = supplies_template_4;
    supplies -= 30;
  } else {
    template_1 = supplies_template_4;
    template_2 = supplies_template_4;
    supplies -= 40;
  }

  for (int i = 0; i < 26; i++) {
    int t1 = template_1[i];
    size_t n = (template_2[i] - template_1[i]) * (supplies * 6554);
    if (n >= 0x8000) t1 += 1;
    resources[(Resource::Type)i] = t1 + (n >> 16);
  }
}

Serf*
Inventory::call_transporter(bool water) {
  Serf *serf = NULL;

  if (water) {
    if (serfs[Serf::TypeSailor] != 0) {
      serf = game->get_serf(serfs[Serf::TypeSailor]);
      serfs[Serf::TypeSailor] = 0;
    } else {
      if ((serfs[Serf::TypeGeneric] != 0) &&
          (resources[Resource::TypeBoat] > 0)) {
        serf = game->get_serf(serfs[Serf::TypeGeneric]);
        serfs[Serf::TypeGeneric] = 0;
        resources[Resource::TypeBoat]--;
        serf->set_type(Serf::TypeSailor);
        generic_count -= 1;
      } else {
        return NULL;
      }
    }
  } else {
    if (serfs[Serf::TypeTransporter] != 0) {
      serf = game->get_serf(serfs[Serf::TypeTransporter]);
      serfs[Serf::TypeTransporter] = 0;
    } else {
      if (serfs[Serf::TypeGeneric] != 0) {
        serf = game->get_serf(serfs[Serf::TypeGeneric]);
        serfs[Serf::TypeGeneric] = 0;
        serf->set_type(Serf::TypeTransporter);
        generic_count -= 1;
      } else {
        return NULL;
      }
    }
  }

  serfs_out += 1;

  return serf;
}

bool
Inventory::call_out_serf(Serf *serf) {
  if (serfs[serf->get_type()] != serf->get_index()) {
    return false;
  }

  serfs[serf->get_type()] = 0;
  if (serf->get_type() == Serf::TypeGeneric) {
    generic_count--;
  }
  serfs_out++;
  return true;
}

Serf*
Inventory::call_out_serf(Serf::Type type) {
  if (serfs[type] == 0) {
    return NULL;
  }

  Serf *serf = game->get_serf(serfs[type]);
  if (!call_out_serf(serf)) {
    return NULL;
  }

  return serf;
}

bool
Inventory::call_internal(Serf *serf) {
  if (serfs[serf->get_type()] != serf->get_index()) {
    return false;
  }

  serfs[serf->get_type()] = 0;

  return true;
}

Serf*
Inventory::call_internal(Serf::Type type) {
  if (serfs[type] == 0) {
    return NULL;
  }

  Serf *serf = game->get_serf(serfs[type]);
  serfs[type] = 0;

  return serf;
}

bool
Inventory::promote_serf_to_knight(Serf *serf) {
  if (serf->get_type() != Serf::TypeGeneric) {
    return false;
  }

  if (resources[Resource::TypeSword] == 0 ||
      resources[Resource::TypeShield] == 0) {
    return false;
  }

  pop_resource(Resource::TypeSword);
  pop_resource(Resource::TypeShield);
  generic_count--;
  serfs[Serf::TypeGeneric] = 0;

  serf->set_type(Serf::TypeKnight0);

  return true;
}

Serf*
Inventory::spawn_serf_generic() {
  Serf *serf = game->get_player(owner)->spawn_serf_generic();

  if (serf != NULL) {
    serf->init_generic(this);

    generic_count++;
    if (serfs[Serf::TypeGeneric] == 0) {
      serfs[Serf::TypeGeneric] = serf->get_index();
    }
  }

  return serf;
}

Resource::Type res_needed[] = {
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

bool
Inventory::specialize_serf(Serf *serf, Serf::Type type) {
  if (serf->get_type() != Serf::TypeGeneric) {
    return false;
  }

  if (serfs[type] != 0) {
    return false;
  }

  if ((res_needed[type*2] != Resource::TypeNone)
      && (resources[res_needed[type*2]] == 0)) {
    return false;
  }
  if ((res_needed[type*2+1] != Resource::TypeNone)
      && (resources[res_needed[type*2+1]] == 0)) {
    return false;
  }

  if (serfs[Serf::TypeGeneric] == serf->get_index()) {
    serfs[Serf::TypeGeneric] = 0;
  }
  generic_count--;

  if (res_needed[type*2] != Resource::TypeNone) {
    resources[res_needed[type*2]]--;
  }
  if (res_needed[type*2+1] != Resource::TypeNone) {
    resources[res_needed[type*2+1]]--;
  }

  serf->set_type(type);

  serfs[type] = serf->get_index();

  return true;
}

Serf*
Inventory::specialize_free_serf(Serf::Type type) {
  if (serfs[Serf::TypeGeneric] == 0) {
    return NULL;
  }

  Serf *serf = game->get_serf(serfs[Serf::TypeGeneric]);

  if (!specialize_serf(serf, type)) {
    return NULL;
  }

  return serf;
}

size_t
Inventory::serf_potential_count(Serf::Type type) {
  size_t count = generic_count;

  if (res_needed[type*2] != Resource::TypeNone) {
    count = std::min(count, resources[res_needed[type*2]]);
  }
  if (res_needed[type*2+1] != Resource::TypeNone) {
    count = std::min(count, resources[res_needed[type*2+1]]);
  }

  return count;
}

void
Inventory::serf_idle_in_stock(Serf *serf) {
  serfs[serf->get_type()] = serf->get_index();
}

void
Inventory::knight_training(Serf *serf, int p) {
  Serf::Type old_type = serf->get_type();
  int r = serf->train_knight(p);
  if (r == 0) serfs[old_type] = 0;

  serf_idle_in_stock(serf);
}

SaveReaderBinary&
operator >> (SaveReaderBinary &reader, Inventory &inventory) {
  uint8_t byte;
  reader >> byte;
  inventory.owner = byte;  // 0
  reader >> byte;
  inventory.res_dir = byte;  // 1
  uint16_t word;
  reader >> word;  // 2
  inventory.flag = word;
  reader >> word;  // 4
  inventory.building = word;

  for (int j = 0; j < 26; j++) {
    reader >> word;  // 6 + 2*j
    inventory.resources[(Resource::Type)j] = word;
  }

  for (int j = 0; j < 2; j++) {
    reader >> byte;  // 58 + j
    inventory.out_queue[j].type = (Resource::Type)(byte-1);
  }

  for (int j = 0; j < 2; j++) {
    reader >> word;  // 60 + 2*j
    inventory.out_queue[j].dest = word;
  }

  reader >> word;  // 64
  inventory.generic_count = word;

  for (int j = 0; j < 27; j++) {
    reader >> word;  // 66 + 2*j
    inventory.serfs[(Serf::Type)j] = word;
  }

  return reader;
}

SaveReaderText&
operator >> (SaveReaderText &reader, Inventory &inventory) {
  reader.value("player") >> inventory.owner;
  reader.value("res_dir") >> inventory.res_dir;
  reader.value("flag") >> inventory.flag;
  reader.value("building") >> inventory.building;

  for (int i = 0; i < 2; i++) {
    reader.value("queue.type")[i] >> inventory.out_queue[i].type;
    reader.value("queue.dest")[i] >> inventory.out_queue[i].dest;
  }

  reader.value("generic_count") >> inventory.generic_count;

  for (int i = 0; i < 26; i++) {
    reader.value("resources")[i] >> inventory.resources[(Resource::Type)i];
    reader.value("serfs")[i] >> inventory.serfs[(Serf::Type)i];
  }
  reader.value("serfs")[26] >> inventory.serfs[(Serf::Type)26];

  return reader;
}

SaveWriterText&
operator << (SaveWriterText &writer, Inventory &inventory) {
  writer.value("player") << inventory.owner;
  writer.value("res_dir") << inventory.res_dir;
  writer.value("flag") << inventory.flag;
  writer.value("building") << inventory.building;

  for (int i = 0; i < 2; i++) {
    writer.value("queue.type") << inventory.out_queue[i].type;
    writer.value("queue.dest") << inventory.out_queue[i].dest;
  }

  writer.value("generic_count") << inventory.generic_count;

  for (int i = 0; i < 26; i++) {
    writer.value("resources") << inventory.resources[(Resource::Type)i];
    writer.value("serfs") << inventory.serfs[(Serf::Type)i];
  }
  writer.value("serfs") << inventory.serfs[(Serf::Type)26];

  return writer;
}
