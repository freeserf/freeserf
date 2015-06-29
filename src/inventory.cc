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

inventory_t::inventory_t(unsigned int index) {
  this->index = index;

  player_num = 0;
  res_dir = 0;
  flag = 0;
  building = 0;
  for (int i = 0; i < 2; i++) {
    out_queue[i].type = RESOURCE_NONE;
    out_queue[i].dest = 0;
  }
  serfs_out = 0;
  generic_count = 0;
  for (int i = 0; i < 26; i++) {
    resources[i] = 0;
    serfs[i] = 0;
  }
  serfs[26] = 0;
}

void
inventory_t::push_resource(resource_type_t resource) {
  resources[resource] += (resources[resource] < 50000) ? 0 : 1;
}

void
inventory_t::get_resource_from_queue(resource_type_t *res, int *dest) {
  *res = out_queue[0].type;
  *dest = out_queue[0].dest;

  out_queue[0].type = out_queue[1].type;
  out_queue[0].dest = out_queue[1].dest;

  out_queue[1].type = RESOURCE_NONE;
  out_queue[1].dest = 0;
}

void
inventory_t::add_to_queue(resource_type_t type, unsigned int dest) {
  if (type == RESOURCE_GROUP_FOOD) {
    /* Select the food resource with highest amount available */
    if (resources[RESOURCE_MEAT] > resources[RESOURCE_BREAD]) {
      if (resources[RESOURCE_MEAT] > resources[RESOURCE_FISH]) {
        type = RESOURCE_MEAT;
      } else {
        type = RESOURCE_FISH;
      }
    } else if (resources[RESOURCE_BREAD] > resources[RESOURCE_FISH]) {
      type = RESOURCE_BREAD;
    } else {
      type = RESOURCE_FISH;
    }
  }

  assert(resources[type] != 0);

  resources[type] -= 1;
  if (out_queue[0].type == RESOURCE_NONE) {
    out_queue[0].type = type;
    out_queue[0].dest = dest;
  } else {
    out_queue[1].type = type;
    out_queue[1].dest = dest;
  }
}

void
inventory_t::reset_queue_for_dest(flag_t *flag) {
  if (out_queue[1].type != RESOURCE_NONE &&
      out_queue[1].dest == flag->get_index()) {
    push_resource(out_queue[1].type);
    out_queue[1].type = RESOURCE_NONE;
  }
  if (out_queue[0].type != RESOURCE_NONE &&
      out_queue[0].dest == flag->get_index()) {
    push_resource(out_queue[0].type);
    out_queue[0].type = out_queue[1].type;
    out_queue[0].dest = out_queue[1].dest;
    out_queue[1].type = RESOURCE_NONE;
  }
}

void
inventory_t::lose_queue() {
  for (int i = 0; i < 2 && out_queue[i].type != RESOURCE_NONE; i++) {
    resource_type_t res = out_queue[i].type;
    int dest = out_queue[i].dest;

    game_cancel_transported_resource(res, dest);
    game_lose_resource(res);
  }
}

void
inventory_t::apply_supplies_preset(int supplies) {
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
    int n = (template_2[i] - template_1[i]) * (supplies * 6554);
    if (n >= 0x8000) t1 += 1;
    resources[i] = t1 + (n >> 16);
  }
}

serf_t*
inventory_t::call_transporter(bool water) {
  serf_t *serf = NULL;

  if (water) {
    if (serfs[SERF_SAILOR] != 0) {
      serf = game.serfs[serfs[SERF_SAILOR]];
      serfs[SERF_SAILOR] = 0;
    } else {
      if ((serfs[SERF_GENERIC] != 0) &&
          (resources[RESOURCE_BOAT] > 0)) {
        serf = game.serfs[serfs[SERF_GENERIC]];
        serfs[SERF_GENERIC] = 0;
        resources[RESOURCE_BOAT]--;
        serf->set_type(SERF_SAILOR);
        generic_count -= 1;
      } else {
        return NULL;
      }
    }
  } else {
    if (serfs[SERF_TRANSPORTER] != 0) {
      serf = game.serfs[serfs[SERF_TRANSPORTER]];
      serfs[SERF_TRANSPORTER] = 0;
    } else {
      if (serfs[SERF_GENERIC] != 0) {
        serf = game.serfs[serfs[SERF_GENERIC]];
        serfs[SERF_GENERIC] = 0;
        serf->set_type(SERF_TRANSPORTER);
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
inventory_t::call_out_serf(serf_t *serf) {
  if (serfs[serf->get_type()] != serf->get_index()) {
    return false;
  }

  serfs[serf->get_type()] = 0;
  if (serf->get_type() == SERF_GENERIC) {
    generic_count--;
  }
  serfs_out++;
  return true;
}

serf_t*
inventory_t::call_out_serf(serf_type_t type) {
  if (serfs[type] == 0) {
    return NULL;
  }

  serf_t *serf = game.serfs[serfs[type]];
  if (!call_out_serf(serf)) {
    return NULL;
  }

  return serf;
}

bool
inventory_t::call_internal(serf_t *serf) {
  if (serfs[serf->get_type()] != serf->get_index()) {
    return false;
  }

  serfs[serf->get_type()] = 0;

  return true;
}

serf_t*
inventory_t::call_internal(serf_type_t type) {
  if (serfs[type] == 0) {
    return NULL;
  }

  serf_t *serf = game.serfs[serfs[type]];
  serfs[type] = 0;

  return serf;
}

bool
inventory_t::promote_serf_to_knight(serf_t *serf) {
  if (serf->get_type() != SERF_GENERIC) {
    return false;
  }

  if (resources[RESOURCE_SWORD] == 0 ||
      resources[RESOURCE_SHIELD] == 0) {
    return false;
  }

  pop_resource(RESOURCE_SWORD);
  pop_resource(RESOURCE_SHIELD);
  generic_count--;
  serfs[SERF_GENERIC] = 0;

  serf->set_type(SERF_KNIGHT_0);

  return true;
}

serf_t*
inventory_t::spawn_serf_generic() {
  serf_t *serf = NULL;
  int r = game.serfs.allocate(&serf, NULL);
  if (r < 0) return NULL;

  serf->init_generic(this);

  generic_count++;
  if (serfs[SERF_GENERIC] == 0) {
    serfs[SERF_GENERIC] = serf->get_index();
  }

  player_t *player = game.player[player_num];
  player->serf_count[SERF_GENERIC] += 1;

  return serf;
}

resource_type_t res_needed[] = {
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_TRANSPORTER = 0,
  RESOURCE_BOAT,    RESOURCE_NONE,    // SERF_SAILOR,
  RESOURCE_SHOVEL,  RESOURCE_NONE,    // SERF_DIGGER,
  RESOURCE_HAMMER,  RESOURCE_NONE,    // SERF_BUILDER,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_TRANSPORTER_INVENTORY,
  RESOURCE_AXE,     RESOURCE_NONE,    // SERF_LUMBERJACK,
  RESOURCE_SAW,     RESOURCE_NONE,    // SERF_SAWMILLER,
  RESOURCE_PICK,    RESOURCE_NONE,    // SERF_STONECUTTER,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_FORESTER,
  RESOURCE_PICK,    RESOURCE_NONE,    // SERF_MINER,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_SMELTER,
  RESOURCE_ROD,     RESOURCE_NONE,    // SERF_FISHER,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_PIGFARMER,
  RESOURCE_CLEAVER, RESOURCE_NONE,    // SERF_BUTCHER,
  RESOURCE_SCYTHE,  RESOURCE_NONE,    // SERF_FARMER,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_MILLER,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_BAKER,
  RESOURCE_HAMMER,  RESOURCE_NONE,    // SERF_BOATBUILDER,
  RESOURCE_HAMMER,  RESOURCE_SAW,     // SERF_TOOLMAKER,
  RESOURCE_HAMMER,  RESOURCE_PINCER,  // SERF_WEAPONSMITH,
  RESOURCE_HAMMER,  RESOURCE_NONE,    // SERF_GEOLOGIST,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_GENERIC,
  RESOURCE_SWORD,   RESOURCE_SHIELD,  // SERF_KNIGHT_0,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_KNIGHT_1,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_KNIGHT_2,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_KNIGHT_3,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_KNIGHT_4,
  RESOURCE_NONE,    RESOURCE_NONE,    // SERF_DEAD
};

bool
inventory_t::specialize_serf(serf_t *serf, serf_type_t type) {
  if (serf->get_type() != SERF_GENERIC) {
    return false;
  }

  if (serfs[type] != 0) {
    return false;
  }

  if ((res_needed[type*2] != RESOURCE_NONE)
      && (resources[res_needed[type*2]] == 0)) {
    return false;
  }
  if ((res_needed[type*2+1] != RESOURCE_NONE)
      && (resources[res_needed[type*2+1]] == 0)) {
    return false;
  }

  if (serfs[SERF_GENERIC] == serf->get_index()) {
    serfs[SERF_GENERIC] = 0;
  }
  generic_count--;

  if (res_needed[type*2] != RESOURCE_NONE) {
    resources[res_needed[type*2]]--;
  }
  if (res_needed[type*2+1] != RESOURCE_NONE) {
    resources[res_needed[type*2+1]]--;
  }

  serf->set_type(type);

  serfs[type] = serf->get_index();

  return true;
}

serf_t*
inventory_t::specialize_free_serf(serf_type_t type) {
  if (serfs[SERF_GENERIC] == 0) {
    return NULL;
  }

  serf_t *serf = game.serfs[serfs[SERF_GENERIC]];

  if (!specialize_serf(serf, type)) {
    return NULL;
  }

  return serf;
}

int
inventory_t::serf_potencial_count(serf_type_t type) {
  int count = generic_count;

  if (res_needed[type*2] != RESOURCE_NONE) {
    count = std::min(count, resources[res_needed[type*2]]);
  }
  if (res_needed[type*2+1] != RESOURCE_NONE) {
    count = std::min(count, resources[res_needed[type*2+1]]);
  }

  return count;
}

void
inventory_t::serf_idle_in_stock(serf_t *serf) {
  serfs[serf->get_type()] = serf->get_index();
}

void
inventory_t::knight_training(serf_t *serf, int p) {
  serf_type_t old_type = serf->get_type();
  int r = serf->train_knight(p);
  if (r == 0) serfs[old_type] = 0;

  serf_idle_in_stock(serf);
}

save_reader_binary_t&
operator >> (save_reader_binary_t &reader, inventory_t &inventory) {
  uint8_t byte;
  reader >> byte;
  inventory.player_num = byte;  // 0
  reader >> byte;
  inventory.res_dir = byte;  // 1
  uint16_t word;
  reader >> word;  // 2
  inventory.flag = word;
  reader >> word;  // 4
  inventory.building = word;

  for (int j = 0; j < 26; j++) {
    reader >> word;  // 6 + 2*j
    inventory.resources[j] = word;
  }

  for (int j = 0; j < 2; j++) {
    reader >> byte;  // 58 + j
    inventory.out_queue[j].type = (resource_type_t)(byte-1);
  }

  for (int j = 0; j < 2; j++) {
    reader >> word;  // 60 + 2*j
    inventory.out_queue[j].dest = word;
  }

  reader >> word;  // 64
  inventory.generic_count = word;

  for (int j = 0; j < 27; j++) {
    reader >> word;  // 66 + 2*j
    inventory.serfs[j] = word;
  }

  return reader;
}

save_reader_text_t&
operator >> (save_reader_text_t &reader, inventory_t &inventory) {
  reader.value("player") >> inventory.player_num;
  reader.value("res_dir") >> inventory.res_dir;
  reader.value("flag") >> inventory.flag;
  reader.value("building") >> inventory.building;

  for (int i = 0; i < 2; i++) {
    reader.value("queue.type")[i] >> inventory.out_queue[i].type;
    reader.value("queue.dest")[i] >> inventory.out_queue[i].dest;
  }

  reader.value("generic_count") >> inventory.generic_count;

  for (int i = 0; i < 26; i++) {
    reader.value("resources")[i] >> inventory.resources[i];
    reader.value("serfs")[i] >> inventory.serfs[i];
  }
  reader.value("serfs")[26] >> inventory.serfs[26];

  return reader;
}

save_writer_text_t&
operator << (save_writer_text_t &writer, inventory_t &inventory) {
  writer.value("player") << inventory.player_num;
  writer.value("res_dir") << inventory.res_dir;
  writer.value("flag") << inventory.flag;
  writer.value("building") << inventory.building;

  for (int i = 0; i < 2; i++) {
    writer.value("queue.type") << inventory.out_queue[i].type;
    writer.value("queue.dest") << inventory.out_queue[i].dest;
  }

  writer.value("generic_count") << inventory.generic_count;

  for (int i = 0; i < 26; i++) {
    writer.value("resources") << inventory.resources[i];
    writer.value("serfs") << inventory.serfs[i];
  }
  writer.value("serfs") << inventory.serfs[26];

  return writer;
}
