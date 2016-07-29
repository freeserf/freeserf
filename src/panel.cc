/*
 * panel.cc - Panel GUI component
 *
 * Copyright (C) 2013-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/panel.h"

#include <cstdlib>

#include "src/game.h"
#include "src/debug.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/interface.h"
#include "src/popup.h"
#include "src/viewport.h"
#include "src/freeserf.h"
#include "src/minimap.h"

PanelBar::PanelBar(Interface *_interface) {
  interface = _interface;

  panel_btns[0] = ButtonBuildInactive;
  panel_btns[1] = ButtonDestroyInactive;
  panel_btns[2] = ButtonMap;
  panel_btns[3] = ButtonStats;
  panel_btns[4] = ButtonSett;

  blink_timer = Timer::create(1, 700, this);
  if (blink_timer != NULL) {
    blink_timer->run();
  }
  blink_trigger = false;
}

PanelBar::~PanelBar() {
  if (blink_timer != NULL) {
    delete blink_timer;
    blink_timer = NULL;
  }
}

/* Draw the frame around action buttons. */
void
PanelBar::draw_panel_frame() {
  const int bottom_svga_layout[] = {
    DATA_FRAME_BOTTOM_SHIELD, 0, 0,
    DATA_FRAME_BOTTOM_BASE+0, 40, 0,
    DATA_FRAME_BOTTOM_BASE+20, 48, 0,

    DATA_FRAME_BOTTOM_BASE+7, 64, 0,
    DATA_FRAME_BOTTOM_BASE+8, 64, 36,
    DATA_FRAME_BOTTOM_BASE+21, 96, 0,

    DATA_FRAME_BOTTOM_BASE+9, 112, 0,
    DATA_FRAME_BOTTOM_BASE+10, 112, 36,
    DATA_FRAME_BOTTOM_BASE+22, 144, 0,

    DATA_FRAME_BOTTOM_BASE+11, 160, 0,
    DATA_FRAME_BOTTOM_BASE+12, 160, 36,
    DATA_FRAME_BOTTOM_BASE+23, 192, 0,

    DATA_FRAME_BOTTOM_BASE+13, 208, 0,
    DATA_FRAME_BOTTOM_BASE+14, 208, 36,
    DATA_FRAME_BOTTOM_BASE+24, 240, 0,

    DATA_FRAME_BOTTOM_BASE+15, 256, 0,
    DATA_FRAME_BOTTOM_BASE+16, 256, 36,
    DATA_FRAME_BOTTOM_BASE+25, 288, 0,

    DATA_FRAME_BOTTOM_TIMERS, 304, 0,
    DATA_FRAME_BOTTOM_SHIELD, 312, 0,
    -1
  };

  /* TODO request full buffer swap(?) */

  const int *layout = bottom_svga_layout;

  /* Draw layout */
  for (int i = 0; layout[i] != -1; i += 3) {
    frame->draw_sprite(layout[i+1], layout[i+2], layout[i]);
  }
}

/* Draw notification icon in action panel. */
void
PanelBar::draw_message_notify() {
  interface->set_msg_flag(2);
  frame->draw_sprite(40, 4, DATA_FRAME_BOTTOM_NOTIFY);
}

/* Draw return arrow icon in action panel. */
void
PanelBar::draw_return_arrow() {
  frame->draw_sprite(40, 28, DATA_FRAME_BOTTOM_ARROW);
}

/* Draw buttons in action panel. */
void
PanelBar::draw_panel_buttons() {
  if (enabled) {
    Player *player = interface->get_player();
    /* Blinking message icon. */
    if ((player != NULL) && player->has_notification()) {
      if (blink_trigger) {
        draw_message_notify();
      }
    }

    /* Return arrow icon. */
    if (interface->get_msg_flag(3)) {
      draw_return_arrow();
    }
  }

  const int inactive_buttons[] = {
    ButtonBuildInactive,
    ButtonDestroyInactive,
    ButtonMapInactive,
    ButtonStatsInactive,
    ButtonSettInactive
  };

  for (int i = 0; i < 5; i++) {
    int button = panel_btns[i];
    if (!enabled) button = inactive_buttons[i];

    int x = 64 + i*48;
    int y = 4;
    int sprite = DATA_PANEL_BUTTON_BASE + button;

    frame->draw_sprite(x, y, sprite);
  }
}

void
PanelBar::internal_draw() {
  draw_panel_frame();
  draw_panel_buttons();
}

/* Handle a click on the panel buttons. */
void
PanelBar::button_click(int button) {
  switch (panel_btns[button]) {
  case ButtonMap:
  case ButtonMapStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildInactive;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapStarred;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettInactive;

      interface->open_popup(PopupBox::TypeMap);

      /* Synchronize minimap window with viewport. */
      Viewport *viewport = interface->get_viewport();
      PopupBox *popup = interface->get_popup_box();
      Minimap *minimap = popup->get_minimap();
      if (minimap != NULL) {
        MapPos pos = viewport->get_current_map_pos();
        minimap->move_to_map_pos(pos);
      }
    }
    break;
  case ButtonSett:
  case ButtonSettStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildInactive;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettStarred;
      interface->open_popup(PopupBox::TypeSettSelect);
    }
    break;
  case ButtonStats:
  case ButtonStatsStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildInactive;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsStarred;
      panel_btns[4] = ButtonSettInactive;
      interface->open_popup(PopupBox::TypeStatSelect);
    }
    break;
  case ButtonBuildRoad:
  case ButtonBuildRoadStarred:
    play_sound(Audio::TypeSfxClick);
    if (interface->is_building_road()) {
      interface->build_road_end();
    } else {
      interface->build_road_begin();
    }
    break;
  case ButtonBuildFlag:
    play_sound(Audio::TypeSfxClick);
    interface->build_flag();
    break;
  case ButtonBuildSmall:
  case ButtonBuildSmallStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildSmallStarred;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettInactive;
      interface->open_popup(PopupBox::TypeBasicBld);
    }
    break;
  case ButtonBuildLarge:
  case ButtonBuildLargeStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildLargeStarred;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettInactive;
      interface->open_popup(PopupBox::TypeBasicBldFlip);
    }
    break;
  case ButtonBuildMine:
  case ButtonBuildMineStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildMineStarred;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettInactive;
      interface->open_popup(PopupBox::TypeMineBuilding);
    }
    break;
  case ButtonDestroy:
    if (interface->get_map_cursor_type() ==
        Interface::CursorTypeRemovableFlag) {
      interface->demolish_object();
    } else {
      panel_btns[0] = ButtonBuildInactive;
      panel_btns[1] = ButtonDestroyInactive;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettInactive;
      interface->open_popup(PopupBox::TypeDemolish);
    }
    break;
  case ButtonBuildCastle:
    interface->build_castle();
    break;
  case ButtonDestroyRoad: {
    bool r = interface->get_player()->get_game()->demolish_road(
                                                interface->get_map_cursor_pos(),
                                                       interface->get_player());
    if (!r) {
      play_sound(Audio::TypeSfxNotAccepted);
      interface->update_map_cursor_pos(interface->get_map_cursor_pos());
    } else {
      play_sound(Audio::TypeSfxAccepted);
    }
  }
    break;
  case ButtonGroundAnalysis:
  case ButtonGroundAnalysisStarred:
    play_sound(Audio::TypeSfxClick);
    if ((interface->get_popup_box() != NULL) &&
        interface->get_popup_box()->is_displayed()) {
      interface->close_popup();
    } else {
      panel_btns[0] = ButtonBuildInactive;
      panel_btns[1] = ButtonGroundAnalysisStarred;
      panel_btns[2] = ButtonMapInactive;
      panel_btns[3] = ButtonStatsInactive;
      panel_btns[4] = ButtonSettInactive;
      interface->open_popup(PopupBox::TypeGroundAnalysis);
    }
    break;
  }
}

bool
PanelBar::handle_click_left(int x, int y) {
  set_redraw();

  if (x >= 41 && x < 53) {
    /* Message bar click */
    if (y < 16) {
      /* Message icon */
      interface->open_message();
    } else if (y >= 28) {
      /* Return arrow */
      interface->return_from_message();
    }
  } else if (x >= 301 && x < 313) {
    /* Timer bar click */
    /* Call to map position */
    unsigned int timer_length = 0;

    if (y < 7) {
      timer_length = 5*60;
    } else if (y < 14) {
      timer_length = 10*60;
    } else if (y < 21) {
      timer_length = 20*60;
    } else if (y < 28) {
      timer_length = 30*60;
    } else {
      timer_length = 60*60;
    }

    interface->get_player()->add_timer(timer_length * TICKS_PER_SEC,
                                       interface->get_map_cursor_pos());

    play_sound(Audio::TypeSfxAccepted);
  } else if (y >= 4 && y < 36 && x >= 64) {
    x -= 64;

    /* Figure out what button was clicked */
    int button = 0;
    while (1) {
      if (x < 32) {
        if (button < 5) {
          break;
        } else {
          return false;
        }
      }
      button += 1;
      if (x < 48) return false;
      x -= 48;
    }
    button_click(button);
  }

  return true;
}

bool
PanelBar::handle_key_pressed(char key, int modifier) {
  if (key < '1' || key > '5') {
    return false;
  }

  button_click(key - '1');

  return true;
}

PanelBar::Button
PanelBar::button_type_with_build_possibility(int build_possibility) {
  Button result;

  switch (build_possibility) {
    case Interface::BuildPossibilityCastle:
      result = ButtonBuildCastle;
      break;
    case Interface::BuildPossibilityMine:
      result = ButtonBuildMine;
      break;
    case Interface::BuildPossibilityLarge:
      result = ButtonBuildLarge;
      break;
    case Interface::BuildPossibilitySmall:
      result = ButtonBuildSmall;
      break;
    case Interface::BuildPossibilityFlag:
      result = ButtonBuildFlag;
      break;
    default:
      result = ButtonBuildInactive;
      break;
  }

  return result;
}

void
PanelBar::update() {
  if ((interface->get_popup_box() != NULL) &&
      interface->get_popup_box()->is_displayed()) {
    switch (interface->get_popup_box()->get_box()) {
      case PopupBox::TypeTransportInfo:
      case PopupBox::TypeOrderedBld:
      case PopupBox::TypeCastleRes:
      case PopupBox::TypeDefenders:
      case PopupBox::TypeMineOutput:
      case PopupBox::TypeBldStock:
      case PopupBox::TypeStartAttack:
      case PopupBox::TypeQuitConfirm:
      case PopupBox::TypeOptions: {
        panel_btns[0] = ButtonBuildInactive;
        panel_btns[1] = ButtonDestroyInactive;
        panel_btns[2] = ButtonMapInactive;
        panel_btns[3] = ButtonStatsInactive;
        panel_btns[4] = ButtonSettInactive;
        break;
      }
      default:
        break;
    }
  } else if (interface->is_building_road()) {
    panel_btns[0] = ButtonBuildRoadStarred;
    panel_btns[1] = ButtonBuildInactive;
    panel_btns[2] = ButtonMapInactive;
    panel_btns[3] = ButtonStatsInactive;
    panel_btns[4] = ButtonSettInactive;
  } else {
    panel_btns[2] = ButtonMap;
    panel_btns[3] = ButtonStats;
    panel_btns[4] = ButtonSett;

    Interface::BuildPossibility build_possibility =
                                             interface->get_build_possibility();

    switch (interface->get_map_cursor_type()) {
      case Interface::CursorTypeNone:
        panel_btns[0] = ButtonBuildInactive;
        if (interface->get_player()->has_castle()) {
          panel_btns[1] = ButtonDestroyInactive;
        } else {
          panel_btns[1] = ButtonGroundAnalysis;
        }
        break;
      case Interface::CursorTypeFlag:
        panel_btns[0] = ButtonBuildRoad;
        panel_btns[1] = ButtonDestroyInactive;
        break;
      case Interface::CursorTypeRemovableFlag:
        panel_btns[0] = ButtonBuildRoad;
        panel_btns[1] = ButtonDestroy;
        break;
      case Interface::CursorTypeBuilding:
        panel_btns[0] = button_type_with_build_possibility(build_possibility);
        panel_btns[1] = ButtonDestroy;
        break;
      case Interface::CursorTypePath:
        panel_btns[0] = ButtonBuildInactive;
        panel_btns[1] = ButtonDestroyRoad;
        if (build_possibility != Interface::BuildPossibilityNone) {
          panel_btns[0] = ButtonBuildFlag;
        }
        break;
      case Interface::CursorTypeClearByFlag:
        if (build_possibility == Interface::BuildPossibilityNone ||
            build_possibility == Interface::BuildPossibilityFlag) {
          panel_btns[0] = ButtonBuildInactive;
          if (interface->get_player()->has_castle()) {
            panel_btns[1] = ButtonDestroyInactive;
          } else {
            panel_btns[1] = ButtonGroundAnalysis;
          }
        } else {
          panel_btns[0] = button_type_with_build_possibility(build_possibility);
          panel_btns[1] = ButtonDestroyInactive;
        }
        break;
      case Interface::CursorTypeClearByPath:
        panel_btns[0] = button_type_with_build_possibility(build_possibility);
        panel_btns[1] = ButtonDestroyInactive;
        break;
      case Interface::CursorTypeClear:
        panel_btns[0] = button_type_with_build_possibility(build_possibility);
        if ((interface->get_player() != NULL) &&
            interface->get_player()->has_castle()) {
          panel_btns[1] = ButtonDestroyInactive;
        } else {
          panel_btns[1] = ButtonGroundAnalysis;
        }
        break;
      default:
        NOT_REACHED();
        break;
    }
  }
  set_redraw();
}

void
PanelBar::on_timer_fired(unsigned int id) {
  blink_trigger = !blink_trigger;
  set_redraw();
}
