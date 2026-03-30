//
// xemu User Interface
//
// Copyright (C) 2020-2022 Matt Borgerson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#pragma once

class PopupMenuItemDelegate;

#if defined(__APPLE__)
#define XEMU_MENU_KBD_SHORTCUT(c) "Cmd+" #c
#else
#define XEMU_MENU_KBD_SHORTCUT(c) "Ctrl+" #c
#endif

// Shared between the desktop menubar and the in-game quick menu (popup).
// When `for_menubar` is true, ImGui::Combo uses visible labels; otherwise
// labels are hidden (##id) so only one control stack appears in the popup.
// When not the menubar, pass `nav` so Help/About can dismiss the overlay first.
void MenuDrawSnapshotSubmenu();
void MenuDrawViewItems(bool for_menubar);
void MenuDrawDebugItems(bool for_menubar);
void MenuDrawHelpItems(bool for_menubar, PopupMenuItemDelegate *nav = nullptr);
