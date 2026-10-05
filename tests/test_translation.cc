/*
 * test_translation.cc - Translations of the texts tests
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

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "src/translation.h"

/* The original English without a language. */
TEST(Translation, English) {
  Translation &translation = Translation::get_instance();
  translation.set_language("");
  EXPECT_EQ(translation.get_language(), "");
  EXPECT_EQ(_("Yes"), "Yes");
  EXPECT_EQ(translation.get_language_name(""), "English");
}

#ifdef FREESERF_TRANSLATIONS
/* The Russian translation of the po folder of the sources. */
TEST(Translation, Russian) {
  Translation &translation = Translation::get_instance();
  std::vector<std::string> languages = translation.get_languages();
  ASSERT_NE(std::find(languages.begin(), languages.end(), "ru"),
            languages.end());

  translation.set_language("ru");
  EXPECT_EQ(translation.get_language(), "ru");
  EXPECT_EQ(_("Yes"), "Да");
  /* The same word in other contexts. */
  EXPECT_EQ(C_("messages", "Few"), "Мало");
  EXPECT_EQ(C_("knight occupation", "Minimum"), "Минимум");
  /* Lines of a text. */
  EXPECT_EQ(_("Do you want\nto quit\nthis game?"),
            "Вы хотите\nвыйти\nиз игры?");
  /* A text without a translation stays English. */
  EXPECT_EQ(_("No translation of this"), "No translation of this");
  translation.set_language("");
}
#endif
