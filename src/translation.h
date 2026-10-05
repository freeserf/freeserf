/*
 * translation.h - Translations of the texts declaration
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

#ifndef SRC_TRANSLATION_H_
#define SRC_TRANSLATION_H_

#include <memory>
#include <string>
#include <vector>

namespace tinygettext {
class DictionaryManager;
}

/* The translations of the texts of the game: gettext .po files of the po
   folder next to the program (in the bundle resources on macOS), read with
   tinygettext. The texts in the sources are the English ones, marked with
   _() where they are shown, N_() in tables; C_() and NC_() with a context
   for the translators when a word alone can be meant in several ways. */
class Translation {
 protected:
  std::unique_ptr<tinygettext::DictionaryManager> manager;
  std::string language;

  Translation();
  void add_directory(const std::string &path);

 public:
  virtual ~Translation();

  static Translation &get_instance();

  /* Codes of the languages with a translation (for example "ru"). */
  std::vector<std::string> get_languages();
  /* The language of the texts, "" for the original English. */
  void set_language(const std::string &code);
  const std::string &get_language() const { return language; }
  /* Name of a language in that language, "English" for "". */
  std::string get_language_name(const std::string &code);

  std::string translate(const char *msgid);
  std::string translate(const char *context, const char *msgid);
};

#define _(msgid) Translation::get_instance().translate(msgid)
#define C_(context, msgid) Translation::get_instance().translate(context, msgid)
#define N_(msgid) msgid
#define NC_(context, msgid) msgid

#endif  // SRC_TRANSLATION_H_
