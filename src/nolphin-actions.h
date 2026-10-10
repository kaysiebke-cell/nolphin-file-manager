/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nolphin
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Nolphin is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nolphin is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NOLPHIN_ACTIONS_H
#define NOLPHIN_ACTIONS_H

#include <glib.h>

#define NOLPHIN_ACTION_STOP "Stop"
#define NOLPHIN_ACTION_RELOAD "Reload"
#define NOLPHIN_ACTION_BACK "Back"
#define NOLPHIN_ACTION_COMPUTER "Computer"
#define NOLPHIN_ACTION_UP "Up"
#define NOLPHIN_ACTION_UP_ACCEL "UpAccel"
#define NOLPHIN_ACTION_UP_ACCEL "UpAccel"
#define NOLPHIN_ACTION_FORWARD "Forward"
#define NOLPHIN_ACTION_SHOW_HIDE_SIDEBAR "Show Hide Sidebar"

#define NOLPHIN_ACTION_SHOW_HIDE_TOOLBAR "Show Hide Toolbar"
#define NOLPHIN_ACTION_TOOLBAR_ALWAYS_SHOW_PATHBAR "Toolbar Always Show Pathbar"
#define NOLPHIN_ACTION_TOOLBAR_ALWAYS_SHOW_ENTRY "Toolbar Always Show Entry"

#define NOLPHIN_ACTION_SHOW_HIDE_STATUSBAR "Show Hide Statusbar"
#define NOLPHIN_ACTION_SHOW_HIDE_MENUBAR "Show Hide Menubar"
#define NOLPHIN_ACTION_SHOW_HIDE_LOCATION_BAR "Show Hide Location Bar"
#define NOLPHIN_ACTION_SHOW_HIDE_EXTRA_PANE "Show Hide Extra Pane"
#define NOLPHIN_ACTION_SPLIT_VIEW_HORIZONTAL "Split View Horizontal"
#define NOLPHIN_ACTION_SPLIT_PANE_1 "Split Pane 1"
#define NOLPHIN_ACTION_SPLIT_PANE_2 "Split Pane 2"
#define NOLPHIN_ACTION_SPLIT_PANE_3 "Split Pane 3"
#define NOLPHIN_ACTION_SPLIT_PANE_4 "Split Pane 4"
#define NOLPHIN_ACTION_SPLIT_PANE_PREVIOUS "Split Pane Previous"
#define NOLPHIN_ACTION_SPLIT_PANE_NUMBERS "Split Pane Numbers"
#define NOLPHIN_ACTION_SPLIT_PANE_SWAP "Split Pane Swap"
#define NOLPHIN_ACTION_SPLIT_LAYOUT_TWO_COLUMNS "Split Layout Two Columns"
#define NOLPHIN_ACTION_SPLIT_LAYOUT_THREE_COLUMNS "Split Layout Three Columns"
#define NOLPHIN_ACTION_SPLIT_LAYOUT_GRID "Split Layout Grid"
#define NOLPHIN_ACTION_SPLIT_LAYOUT_BIG_PLUS_TWO "Split Layout Big Plus Two"
#define NOLPHIN_ACTION_SPLIT_LAYOUT_TWO_ROWS "Split Layout Two Rows"
#define NOLPHIN_ACTION_DUPLICATE_TAB "Duplicate Tab"
#define NOLPHIN_ACTION_CLOSE_ALL_TABS "Close All Tabs"
#define NOLPHIN_ACTION_RESTORE_CLOSED_TAB "Restore Closed Tab"
#define NOLPHIN_ACTION_SHOW_HIDE_TERMINAL "Show Hide Terminal"
#define NOLPHIN_ACTION_SHOW_HIDE_PREVIEW "Show Hide Preview"
#define NOLPHIN_ACTION_GO_TO_BURN_CD "Go to Burn CD"
#define NOLPHIN_ACTION_EDIT_LOCATION "Edit Location"
#define NOLPHIN_ACTION_COMPACT_VIEW "CompactView"
#define NOLPHIN_ACTION_ICON_VIEW "IconView"
#define NOLPHIN_ACTION_LIST_VIEW "ListView"
#define NOLPHIN_ACTION_GALLERY_VIEW "GalleryView"
#define NOLPHIN_ACTION_GO_HOME "Home"
#define NOLPHIN_ACTION_ADD_BOOKMARK "Add Bookmark"
#define NOLPHIN_ACTION_EDIT_BOOKMARKS "Edit Bookmarks"
#define NOLPHIN_ACTION_HOME "Home"
#define NOLPHIN_ACTION_ZOOM_IN "Zoom In"
#define NOLPHIN_ACTION_ZOOM_OUT "Zoom Out"
#define NOLPHIN_ACTION_ZOOM_NORMAL "Zoom Normal"
#define NOLPHIN_ACTION_SHOW_HIDDEN_FILES "Show Hidden Files"
#define NOLPHIN_ACTION_CLOSE "Close"
#define NOLPHIN_ACTION_SEARCH "Search"
#define NOLPHIN_ACTION_FOLDER_WINDOW "Folder Window"
#define NOLPHIN_ACTION_NEW_TAB "New Tab"

#define NOLPHIN_ACTION_OPEN "Open"
#define NOLPHIN_ACTION_OPEN_ALTERNATE "OpenAlternate"
#define NOLPHIN_ACTION_OPEN_IN_NEW_TAB "OpenInNewTab"
#define NOLPHIN_ACTION_LOCATION_OPEN_ALTERNATE "LocationOpenAlternate"
#define NOLPHIN_ACTION_LOCATION_OPEN_IN_NEW_TAB "LocationOpenInNewTab"
#define NOLPHIN_ACTION_OTHER_APPLICATION1 "OtherApplication1"
#define NOLPHIN_ACTION_OTHER_APPLICATION2 "OtherApplication2"
#define NOLPHIN_ACTION_NEW_FOLDER "New Folder"
#define NOLPHIN_ACTION_PROPERTIES "Properties"
#define NOLPHIN_ACTION_LOCATION_PROPERTIES "LocationProperties"
#define NOLPHIN_ACTION_NO_TEMPLATES "No Templates"
#define NOLPHIN_ACTION_EMPTY_TRASH "Empty Trash"
#define NOLPHIN_ACTION_CUT "Cut"
#define NOLPHIN_ACTION_LOCATION_CUT "LocationCut"
#define NOLPHIN_ACTION_COPY "Copy"
#define NOLPHIN_ACTION_LOCATION_COPY "LocationCopy"
#define NOLPHIN_ACTION_PASTE "Paste"
#define NOLPHIN_ACTION_PASTE_FILES_INTO "Paste Files Into"
#define NOLPHIN_ACTION_COPY_TO_NEXT_PANE "Copy to next pane"
#define NOLPHIN_ACTION_MOVE_TO_NEXT_PANE "Move to next pane"
#define NOLPHIN_ACTION_COPY_TO_HOME "Copy to Home"
#define NOLPHIN_ACTION_MOVE_TO_HOME "Move to Home"
#define NOLPHIN_ACTION_COPY_TO_DESKTOP "Copy to Desktop"
#define NOLPHIN_ACTION_MOVE_TO_DESKTOP "Move to Desktop"
#define NOLPHIN_ACTION_BROWSE_MOVE_TO "BrowseMoveTo"
#define NOLPHIN_ACTION_BROWSE_COPY_TO "BrowseCopyTo"
#define NOLPHIN_ACTION_COPY_TO_MENU "CopyToMenu"
#define NOLPHIN_ACTION_MOVE_TO_MENU "MoveToMenu"
#define NOLPHIN_ACTION_LOCATION_PASTE_FILES_INTO "LocationPasteFilesInto"
#define NOLPHIN_ACTION_RENAME "Rename"
#define NOLPHIN_ACTION_BATCH_RENAME "Massenumbenennung"
#define NOLPHIN_ACTION_DUPLICATE "Duplicate"
#define NOLPHIN_ACTION_CREATE_LINK "Create Link"
#define NOLPHIN_ACTION_SELECT_ALL "Select All"
#define NOLPHIN_ACTION_INVERT_SELECTION "Invert Selection"
#define NOLPHIN_ACTION_SELECT_PATTERN "Select Pattern"
#define NOLPHIN_ACTION_SAVE_SELECTION "SaveSelection"
#define NOLPHIN_ACTION_RESTORE_SELECTION "RestoreSelection"
#define NOLPHIN_ACTION_TRASH "Trash"
#define NOLPHIN_ACTION_LOCATION_TRASH "LocationTrash"
#define NOLPHIN_ACTION_DELETE "Delete"
#define NOLPHIN_ACTION_LOCATION_DELETE "LocationDelete"
#define NOLPHIN_ACTION_RESTORE_FROM_TRASH "Restore From Trash"
#define NOLPHIN_ACTION_LOCATION_RESTORE_FROM_TRASH "LocationRestoreFromTrash"
#define NOLPHIN_ACTION_CONNECT_TO_SERVER_LINK "Connect To Server Link"
#define NOLPHIN_ACTION_MOUNT_VOLUME "Mount Volume"
#define NOLPHIN_ACTION_UNMOUNT_VOLUME "Unmount Volume"
#define NOLPHIN_ACTION_EJECT_VOLUME "Eject Volume"
#define NOLPHIN_ACTION_START_VOLUME "Start Volume"
#define NOLPHIN_ACTION_STOP_VOLUME "Stop Volume"
#define NOLPHIN_ACTION_POLL "Poll"
#define NOLPHIN_ACTION_SELF_MOUNT_VOLUME "Self Mount Volume"
#define NOLPHIN_ACTION_SELF_UNMOUNT_VOLUME "Self Unmount Volume"
#define NOLPHIN_ACTION_SELF_EJECT_VOLUME "Self Eject Volume"
#define NOLPHIN_ACTION_SELF_START_VOLUME "Self Start Volume"
#define NOLPHIN_ACTION_SELF_STOP_VOLUME "Self Stop Volume"
#define NOLPHIN_ACTION_SELF_POLL "Self Poll"
#define NOLPHIN_ACTION_LOCATION_MOUNT_VOLUME "Location Mount Volume"
#define NOLPHIN_ACTION_LOCATION_UNMOUNT_VOLUME "Location Unmount Volume"
#define NOLPHIN_ACTION_LOCATION_EJECT_VOLUME "Location Eject Volume"
#define NOLPHIN_ACTION_LOCATION_START_VOLUME "Location Start Volume"
#define NOLPHIN_ACTION_LOCATION_STOP_VOLUME "Location Stop Volume"
#define NOLPHIN_ACTION_LOCATION_POLL "Location Poll"
#define NOLPHIN_ACTION_SCRIPTS "Scripts"
#define NOLPHIN_ACTION_ACTIONS "Actions"
#define NOLPHIN_ACTION_NEW_DOCUMENTS "New Documents"
#define NOLPHIN_ACTION_NEW_EMPTY_DOCUMENT "New Empty Document"
#define NOLPHIN_ACTION_EMPTY_TRASH_CONDITIONAL "Empty Trash Conditional"
#define NOLPHIN_ACTION_MANUAL_LAYOUT "Manual Layout"
#define NOLPHIN_ACTION_REVERSED_ORDER "Reversed Order"
#define NOLPHIN_ACTION_CLEAN_UP "Clean Up"
#define NOLPHIN_ACTION_KEEP_ALIGNED "Keep Aligned"
#define NOLPHIN_ACTION_ARRANGE_ITEMS "Arrange Items"
#define NOLPHIN_ACTION_STRETCH "Stretch"
#define NOLPHIN_ACTION_UNSTRETCH "Unstretch"
#define NOLPHIN_ACTION_ZOOM_ITEMS "Zoom Items"
#define NOLPHIN_ACTION_SORT_TRASH_TIME "Sort by Trash Time"
#define NOLPHIN_ACTION_OPEN_AS_ROOT "OpenAsRoot"
#define NOLPHIN_ACTION_TOGGLE_LOCATION "Toggle Location Button"

#define NOLPHIN_ACTION_STATUSBAR_PLACES "Statusbar Places"
#define NOLPHIN_ACTION_STATUSBAR_TREEVIEW "Statusbar Treeview"
#define NOLPHIN_ACTION_STATUSBAR_SIDEBAR_TOGGLE "Statusbar Sidebar Toggle"

#define NOLPHIN_ACTION_OPEN_IN_TERMINAL "OpenInTerminal"
#define NOLPHIN_ACTION_GID_ADD_PROJECT "GIDAddProject"
#define NOLPHIN_ACTION_FOLLOW_SYMLINK "FollowSymbolicLink"
#define NOLPHIN_ACTION_OPEN_CONTAINING_FOLDER "OpenContainingFolder"
#define NOLPHIN_ACTION_COMPRESS "Compress"
#define NOLPHIN_ACTION_EXTRACT_HERE "ExtractHere"
#define NOLPHIN_ACTION_TEST_ARCHIVE "TestArchive"
#define NOLPHIN_ACTION_CHECKSUM "ComputeChecksum"
#define NOLPHIN_ACTION_ENCRYPT "Encrypt"
#define NOLPHIN_ACTION_DECRYPT "Decrypt"
#define NOLPHIN_ACTION_EDIT_ACL "EditAcl"
#define NOLPHIN_ACTION_GIT_MENU "GitMenu"
#define NOLPHIN_ACTION_GIT_STATUS "GitStatus"
#define NOLPHIN_ACTION_GIT_ADD "GitAdd"
#define NOLPHIN_ACTION_GIT_COMMIT "GitCommit"
#define NOLPHIN_ACTION_GIT_PULL "GitPull"
#define NOLPHIN_ACTION_GIT_PUSH "GitPush"
#define NOLPHIN_ACTION_GIT_LOG "GitLog"
#define NOLPHIN_ACTION_GIT_DIFF "GitDiff"
#define NOLPHIN_ACTION_GIT_REMOTE_ADD "GitRemoteAdd"
#define NOLPHIN_ACTION_GIT_CLONE "GitClone"
#define NOLPHIN_ACTION_GIT_COMPARE "GitCompare"
#define NOLPHIN_ACTION_GIT_SYNC "GitSync"

#define NOLPHIN_ACTION_METADATA_MENU "MetadataMenu"
#define NOLPHIN_ACTION_EDIT_TAGS "EditTags"
#define NOLPHIN_ACTION_EDIT_COMMENT "EditComment"
#define NOLPHIN_ACTION_EDIT_EMBLEM "EditEmblem"
#define NOLPHIN_ACTION_RATING_MENU "RatingMenu"
#define NOLPHIN_ACTION_RATING_0 "Rating0"
#define NOLPHIN_ACTION_RATING_1 "Rating1"
#define NOLPHIN_ACTION_RATING_2 "Rating2"
#define NOLPHIN_ACTION_RATING_3 "Rating3"
#define NOLPHIN_ACTION_RATING_4 "Rating4"
#define NOLPHIN_ACTION_RATING_5 "Rating5"

#define NOLPHIN_ACTION_PLUGIN_MANAGER "NolphinPluginManager"

#define NOLPHIN_ACTION_SHOW_THUMBNAILS "Show Thumbnails"
#define NOLPHIN_ACTION_SHOW_FULL_CONTEXT_MENU "ShowFullContextMenu"

#define NOLPHIN_ACTION_PIN_FILE        "Pin File"
#define NOLPHIN_ACTION_UNPIN_FILE      "Unpin File"
#define NOLPHIN_ACTION_FAVORITE_FILE        "Favorite File"
#define NOLPHIN_ACTION_UNFAVORITE_FILE      "Unfavorite File"
#define NOLPHIN_ACTION_DESKTOP_OVERLAY "Desktop Overlay"

#define NOLPHIN_ACTION_SIDEBAR_REMOVE "Remove Bookmark"
#define NOLPHIN_ACTION_SIDEBAR_DETECT_MEDIA "Detect Media"

typedef struct
{
    const gchar  *action_name; // The action's name
    const gchar  *config_widget_name; // The builder id of the corresponding toggle in preferences
                                      // Can be null if the action's should be tied to another
                                      // action's visibility.
    const gchar  *ui_path; // The xml path of the item from the ui file
    const gchar  *settings_key;; // The gsettings key corresponding to the menu item's visibility.
} ConfigurableMenuItemInfo;

static const ConfigurableMenuItemInfo CONFIGURABLE_MENU_ITEM_INFO [] = {
    // Selection
    { NOLPHIN_ACTION_OPEN, "selection_menu__open_check",
     "/selection/Open Placeholder/Open", "selection-menu-open" },

    { NOLPHIN_ACTION_OPEN_IN_NEW_TAB, "selection_menu__open_in_new_tab_check",
     "/selection/Open Placeholder/OpenInNewTab", "selection-menu-open-in-new-tab" },

    { NOLPHIN_ACTION_OPEN_ALTERNATE, "selection_menu__open_in_new_window_check",
     "/selection/Open Placeholder/OpenAlternate", "selection-menu-open-in-new-window" },

    { NOLPHIN_ACTION_SCRIPTS, "selection_menu__scripts_check",
     "/selection/Open Placeholder/Scripts", "selection-menu-scripts" },

    { NOLPHIN_ACTION_CUT, "selection_menu__cut_check",
     "/selection/File Clipboard Actions/Cut", "selection-menu-cut" },

    { NOLPHIN_ACTION_COPY, "selection_menu__copy_check",
     "/selection/File Clipboard Actions/Copy", "selection-menu-copy" },

    { NOLPHIN_ACTION_PASTE_FILES_INTO, "selection_menu__paste_check",
     "/selection/File Clipboard Actions/Paste Files Into", "selection-menu-paste" },

    { NOLPHIN_ACTION_DUPLICATE, "selection_menu__duplicate_check",
     "/selection/File Clipboard Actions/Duplicate", "selection-menu-duplicate" },

    { NOLPHIN_ACTION_PIN_FILE, "selection_menu__pin_check",
     "/selection/File Actions/Pin File", "selection-menu-pin" },
    { NOLPHIN_ACTION_UNPIN_FILE, NULL,
     "/selection/File Actions/Unpin File", "selection-menu-pin" },

    { NOLPHIN_ACTION_FAVORITE_FILE, "selection_menu__favorite_check",
     "/selection/File Actions/Favorite File", "selection-menu-favorite" },
    { NOLPHIN_ACTION_UNFAVORITE_FILE, NULL,
     "/selection/File Actions/Unfavorite File", "selection-menu-favorite" },

    { NOLPHIN_ACTION_CREATE_LINK, "selection_menu__make_link_check",
     "/selection/File Actions/Create Link", "selection-menu-make-link" },

    { NOLPHIN_ACTION_RENAME, "selection_menu__rename_check",
     "/selection/File Actions/Rename", "selection-menu-rename" },

    { NOLPHIN_ACTION_COPY_TO_MENU, "selection_menu__copy_to_check",
     "/selection/File Actions/CopyToMenu", "selection-menu-copy-to" },

    { NOLPHIN_ACTION_MOVE_TO_MENU, "selection_menu__move_to_check",
     "/selection/File Actions/MoveToMenu", "selection-menu-move-to" },

    { NOLPHIN_ACTION_OPEN_IN_TERMINAL, "selection_menu__open_in_terminal_check",
     "/selection/OpenInTerminal", "selection-menu-open-in-terminal" },

    { NOLPHIN_ACTION_OPEN_AS_ROOT, "selection_menu__open_as_root_check",
     "/selection/OpenAsRoot", "selection-menu-open-as-root" },

    { NOLPHIN_ACTION_TRASH, "selection_menu__move_to_trash_check",
     "/selection/Dangerous File Actions/Trash", "selection-menu-move-to-trash" },

    { NOLPHIN_ACTION_PROPERTIES, "selection_menu__properties_check",
     "/selection/Properties", "selection-menu-properties" },

     // Background

    { NOLPHIN_ACTION_NEW_FOLDER, "background_menu__create_new_folder_check",
     "/background/Before Zoom Items/New Object Items/New Folder", "background-menu-create-new-folder" },

    { NOLPHIN_ACTION_SCRIPTS, "background_menu__scripts_check",
     "/background/Before Zoom Items/New Object Items/Scripts", "background-menu-scripts" },

    { NOLPHIN_ACTION_OPEN_IN_TERMINAL, "background_menu__open_in_terminal_check",
     "/background/Before Zoom Items/OpenInTerminal", "background-menu-open-in-terminal" },

    { NOLPHIN_ACTION_OPEN_AS_ROOT, "background_menu__open_as_root_check",
     "/background/Before Zoom Items/OpenAsRoot", "background-menu-open-as-root" },

    { NOLPHIN_ACTION_SHOW_HIDDEN_FILES, "background_menu__show_hidden_files_check",
     "/background/Before Zoom Items/Show Hidden Files", "background-menu-show-hidden-files" },

    { NOLPHIN_ACTION_PASTE, "background_menu__paste_check",
     "/background/Before Zoom Items/File Clipboard Actions/Paste", "background-menu-paste" },

    { NOLPHIN_ACTION_PROPERTIES, "background_menu__properties_check",
     "/background/Folder Items Placeholder/Properties", "background-menu-properties" },

     // Icon View (merged with background)
    { NOLPHIN_ACTION_ARRANGE_ITEMS, "iconview_menu__arrange_items_check",
     "/background/Before Zoom Items/View Items/Arrange Items", "iconview-menu-arrange-items" },

    { NOLPHIN_ACTION_CLEAN_UP, "iconview_menu__organize_by_name_check",
     "/background/Before Zoom Items/View Items/Clean Up", "iconview-menu-organize-by-name" },

     // Desktop (new)
    { NOLPHIN_ACTION_DESKTOP_OVERLAY, "desktop_menu__customize_check",
     "/background/Before Zoom Items/View Items/Desktop Overlay", "desktop-menu-customize" },
};

#define CONFIGURABLE_MENU_ITEM_COUNT (G_N_ELEMENTS (CONFIGURABLE_MENU_ITEM_INFO))

#endif /* NOLPHIN_ACTIONS_H */
