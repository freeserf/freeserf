/*
 * translation.cc - Translations of the texts implementation (tinygettext)
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

#include <SDL3/SDL.h>

#include <exception>
#include <set>

#include <tinygettext/log.hpp>
#include <tinygettext/tinygettext.hpp>
#include <tinygettext/unix_file_system.hpp>

#include "src/log.h"

static void
log_warning(const std::string &message) {
  Log::Warn["translation"] << message;
}

static void
log_error(const std::string &message) {
  Log::Error["translation"] << message;
}

Translation::Translation() {
  tinygettext::Log::set_log_info_callback(nullptr);
  tinygettext::Log::set_log_warning_callback(log_warning);
  tinygettext::Log::set_log_error_callback(log_error);

  manager.reset(new tinygettext::DictionaryManager(
    std::unique_ptr<tinygettext::FileSystem>(
      new tinygettext::UnixFileSystem())));
  const char *base = SDL_GetBasePath();
  if (base != nullptr) {
    add_directory(std::string(base) + "po");
  }
#ifdef FREESERF_PO_DIR
  /* Running from the build tree. */
  add_directory(FREESERF_PO_DIR);
#endif
}

/* tinygettext throws on a directory that does not exist. */
void
Translation::add_directory(const std::string &path) {
  SDL_PathInfo info;
  if (SDL_GetPathInfo(path.c_str(), &info) &&
      info.type == SDL_PATHTYPE_DIRECTORY) {
    manager->add_directory(path);
  }
}

Translation::~Translation() {
}

Translation &
Translation::get_instance() {
  static Translation translation;
  return translation;
}

std::vector<std::string>
Translation::get_languages() {
  std::vector<std::string> result;
  try {
    for (const tinygettext::Language &lang : manager->get_languages()) {
      result.push_back(lang.str());
    }
  } catch (const std::exception &e) {
    Log::Warn["translation"] << "Failed to look for translations: "
                             << e.what();
  }
  return result;
}

void
Translation::set_language(const std::string &code) {
  tinygettext::Language lang;
  if (!code.empty()) {
    lang = tinygettext::Language::from_name(code);
    if (!lang) {
      Log::Warn["translation"] << "Unknown language " << code;
    }
  }
  manager->set_language(lang);
  language = lang ? code : std::string();
}

std::string
Translation::get_language_name(const std::string &code) {
  if (code.empty()) {
    return "English";
  }
  tinygettext::Language lang = tinygettext::Language::from_name(code);
  if (!lang) {
    return code;
  }
  std::string name = lang.get_localized_name();
  return name.empty() ? lang.get_name() : name;
}

std::string
Translation::translate(const char *msgid) {
  if (language.empty()) {
    return msgid;
  }
  try {
    return manager->get_dictionary().translate(msgid);
  } catch (const std::exception &e) {
    Log::Warn["translation"] << "Failed to read the translation: " << e.what();
    language.clear();
    return msgid;
  }
}

std::string
Translation::translate(const char *context, const char *msgid) {
  if (language.empty()) {
    return msgid;
  }
  try {
    return manager->get_dictionary().translate_ctxt(context, msgid);
  } catch (const std::exception &e) {
    Log::Warn["translation"] << "Failed to read the translation: " << e.what();
    language.clear();
    return msgid;
  }
}
