/*
 * characters.cc - Characters implementation
 *
 * Copyright (C) 2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/characters.h"

#include <vector>

std::vector<Character> character_presets = {
  Character({ 0, "ERROR", "ERROR"}),
  Character({ 1, "Lady Amalie",
    "An inoffensive lady, reserved, who goes about her work peacefully."}),
  Character({ 2, "Kumpy Onefinger",
    "A very hostile character, who loves gold above all else."}),
  Character({ 3, "Balduin, a former monk",
    "A very discrete character, who worries chiefly about the protection of his"
    " lands and his important buildings."}),
  Character({ 4, "Frollin",
    "His unpredictable behaviour will always take you by surprise. "
    "He will \"pilfer\" away lands that are not occupied."}),
  Character({ 5, "Kallina",
    "She is a fighter who attempts to block the enemy’s food supply by using "
    "strategic tactics."}),
  Character({ 6, "Rasparuk the druid",
    "His tactics consist in amassing large stocks of raw materials. "
    "But he attacks slyly."}),
  Character({ 7, "Count Aldaba",
    "Protect your warehouses well, because he is aggressive and knows exactly "
    "where he must attack."}),
  Character({ 8, "The King Rolph VII",
    "He is a prudent ruler, without any particular weakness. He will try to "
    "check the supply of construction materials of his adversaries."}),
  Character({ 9, "Homen Doublehorn",
    "He is the most aggressive enemy. Watch your buildings carefully, "
    "otherwise he might take you by surprise."}),
  Character({10, "Sollok the Joker",
    "A sly and repugnant adversary, he will try to stop the supply of raw "
    "materials of his enemies right from the beginning of the game."}),
  Character({11, "Enemy", "Last enemy."}),
  Character({12, "You", "You."}),
  Character({13, "Friend", "Your partner."})
};

Characters
Characters::characters;

size_t
Characters::get_count() const {
  return character_presets.size();
}

Character &
Characters::get_character(size_t index) const {
  return character_presets[index];
}
