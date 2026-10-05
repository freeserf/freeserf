/*
 * translation-dummy.cc - Translations of the texts implementation (none)
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/translation.h"

/* Without tinygettext there is no dictionary. */
namespace tinygettext {
class DictionaryManager {};
}

Translation::Translation() {
}

Translation::~Translation() {
}

void
Translation::add_directory(const std::string &path) {
}

Translation &
Translation::get_instance() {
  static Translation translation;
  return translation;
}

std::vector<std::string>
Translation::get_languages() {
  return std::vector<std::string>();
}

void
Translation::set_language(const std::string &code) {
}

std::string
Translation::get_language_name(const std::string &code) {
  return code.empty() ? "English" : code;
}

std::string
Translation::translate(const char *msgid) {
  return msgid;
}

std::string
Translation::translate(const char *context, const char *msgid) {
  return msgid;
}
