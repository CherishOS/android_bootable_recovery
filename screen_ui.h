/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RECOVERY_SCREEN_UI_H
#define RECOVERY_SCREEN_UI_H

#include <stdio.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ui.h"

// From minui/minui.h.
struct GRSurface;

// This class maintains the menu selection and display of the screen ui.
class Menu {
 public:
  // Constructs a Menu instance with the given |headers|, |items| and properties. Sets the initial
  // selection to |initial_selection|.
  Menu(bool scrollable, size_t max_items, size_t max_length,
       const std::vector<std::string>& headers, const std::vector<std::string>& items,
       size_t initial_selection);

  bool scrollable() const {
    return scrollable_;
  }

  size_t selection() const {
    return selection_;
  }

  // Returns count of menu items.
  size_t ItemsCount() const;

  // Returns the index of the first menu item.
  size_t MenuStart() const;

  // Returns the index of the last menu item + 1.
  size_t MenuEnd() const;

  // Menu example:
  // info:                           Android Recovery
  //                                 ....
  // help messages:                  Swipe up/down to move
  //                                 Swipe left/right to select
  // empty line (horizontal rule):
  // menu headers:                   Select file to view
  // menu items:                     /cache/recovery/last_log
  //                                 /cache/recovery/last_log.1
  //                                 /cache/recovery/last_log.2
  //                                 ...
  const std::vector<std::string>& text_headers() const;
  std::string TextItem(size_t index) const;

  // Checks if the menu items fit vertically on the screen. Returns true and set the
  // |cur_selection_str| if the items exceed the screen limit.
  bool ItemsOverflow(std::string* cur_selection_str) const;

  // Sets the current selection to |sel|. Handle the overflow cases depending on if the menu is
  // scrollable.
  int Select(int sel);

 private:
  // The menu is scrollable to display more items. Used on wear devices who have smaller screens.
  const bool scrollable_;
  // The max number of menu items to fit vertically on a screen.
  const size_t max_display_items_;
  // The length of each item to fit horizontally on a screen.
  const size_t max_item_length_;
  // The menu headers.
  std::vector<std::string> text_headers_;
  // The actual menu items trimmed to fit the given properties.
  std::vector<std::string> text_items_;
  // The first item to display on the screen.
  size_t menu_start_;
  // Current menu selection.
  size_t selection_;
};

// Implementation of RecoveryUI appropriate for devices with a screen
// (shows an icon + a progress bar, text logging, menu, etc.)
class ScreenRecoveryUI : public RecoveryUI {
 public:
  enum UIElement {
    HEADER,
    MENU,
    MENU_SEL_BG,
    MENU_SEL_BG_ACTIVE,
    MENU_SEL_FG,
    LOG,
    TEXT_FILL,
    INFO
  };

  ScreenRecoveryUI();
  explicit ScreenRecoveryUI(bool scrollable_menu);
  ~ScreenRecoveryUI() override;

  bool Init(const std::string& locale) override;
  std::string GetLocale() const override;

  // overall recovery state ("background image")
  void SetBackground(Icon icon) override;
  void SetSystemUpdateText(bool security_update) override;

  // progress indicator
  void SetProgressType(ProgressType type) override;
  void ShowProgress(float portion, float seconds) override;
  void SetProgress(float fraction) override;

  void SetStage(int current, int max) override;

  // text log
  void ShowText(bool visible) override;
  bool IsTextVisible() override;
  bool WasTextEverVisible() override;

  // printing messages
  void Print(const char* fmt, ...) override __printflike(2, 3);
  void PrintOnScreenOnly(const char* fmt, ...) override __printflike(2, 3);
  void ShowFile(const std::string& filename) override;

  // menu display
  size_t ShowMenu(const std::vector<std::string>& headers, const std::vector<std::string>& items,
                  size_t initial_selection, bool menu_only,
                  const std::function<int(int, bool)>& key_handler) override;
  void SetTitle(const std::vector<std::string>& lines) override;

  void KeyLongPress(int) override;

  void Redraw();

  void SetColor(UIElement e) const;

  // Checks the background text image, for debugging purpose. It iterates the locales embedded in
  // the on-device resource files and shows the localized text, for manual inspection.
  void CheckBackgroundTextImages();

 protected:
  // The margin that we don't want to use for showing texts (e.g. round screen, or screen with
  // rounded corners).
  const int margin_width_;
  const int margin_height_;

  // Number of frames per sec (default: 30) for both parts of the animation.
  const int animation_fps_;

  // The scale factor from dp to pixels. 1.0 for mdpi, 4.0 for xxxhdpi.
  const float density_;

  virtual bool InitTextParams();

  // Displays some header text followed by a menu of items, which appears at the top of the screen
  // (in place of any scrolling ui_print() output, if necessary).
  virtual void StartMenu(const std::vector<std::string>& headers,
                         const std::vector<std::string>& items, size_t initial_selection);

  // Sets the menu highlight to the given index, wrapping if necessary. Returns the actual item
  // selected.
  virtual int SelectMenu(int sel);

  // Ends menu mode, resetting the text overlay so that ui_print() statements will be displayed.
  virtual void EndMenu();

  virtual void draw_background_locked();
  virtual void draw_foreground_locked();
  virtual void draw_screen_locked();
  virtual void draw_menu_and_text_buffer_locked(const std::vector<std::string>& help_message);
  virtual void update_screen_locked();
  virtual void update_progress_locked();

  GRSurface* GetCurrentFrame() const;
  GRSurface* GetCurrentText() const;

  void ProgressThreadLoop();

  virtual void ShowFile(FILE*);
  virtual void PrintV(const char*, bool, va_list);
  void PutChar(char);
  void ClearText();

  void LoadAnimation();
  void LoadBitmap(const char* filename, GRSurface** surface);
  void LoadLocalizedBitmap(const char* filename, GRSurface** surface);

  int PixelsFromDp(int dp) const;
  virtual int GetAnimationBaseline() const;
  virtual int GetProgressBaseline() const;
  virtual int GetTextBaseline() const;

  // Returns pixel width of draw buffer.
  virtual int ScreenWidth() const;
  // Returns pixel height of draw buffer.
  virtual int ScreenHeight() const;

  // Draws a highlight bar at (x, y) - (x + width, y + height).
  virtual void DrawHighlightBar(int x, int y, int width, int height) const;
  // Draws a horizontal rule at Y. Returns the offset it should be moving along Y-axis.
  virtual int DrawHorizontalRule(int y) const;
  // Draws a line of text. Returns the offset it should be moving along Y-axis.
  virtual int DrawTextLine(int x, int y, const std::string& line, bool bold) const;
  // Draws surface portion (sx, sy, w, h) at screen location (dx, dy).
  virtual void DrawSurface(GRSurface* surface, int sx, int sy, int w, int h, int dx, int dy) const;
  // Draws rectangle at (x, y) - (x + w, y + h).
  virtual void DrawFill(int x, int y, int w, int h) const;
  // Draws given surface (surface->pixel_bytes = 1) as text at (x, y).
  virtual void DrawTextIcon(int x, int y, GRSurface* surface) const;
  // Draws multiple text lines. Returns the offset it should be moving along Y-axis.
  int DrawTextLines(int x, int y, const std::vector<std::string>& lines) const;
  // Similar to DrawTextLines() to draw multiple text lines, but additionally wraps long lines. It
  // keeps symmetrical margins of 'x' at each end of a line. Returns the offset it should be moving
  // along Y-axis.
  int DrawWrappedTextLines(int x, int y, const std::vector<std::string>& lines) const;

  Icon currentIcon;

  // The layout to use.
  int layout_;

  GRSurface* error_icon;

  GRSurface* erasing_text;
  GRSurface* error_text;
  GRSurface* installing_text;
  GRSurface* no_command_text;

  GRSurface** introFrames;
  GRSurface** loopFrames;

  GRSurface* progressBarEmpty;
  GRSurface* progressBarFill;
  GRSurface* stageMarkerEmpty;
  GRSurface* stageMarkerFill;

  ProgressType progressBarType;

  float progressScopeStart, progressScopeSize, progress;
  double progressScopeTime, progressScopeDuration;

  // true when both graphics pages are the same (except for the progress bar).
  bool pagesIdentical;

  size_t text_cols_, text_rows_;

  // Log text overlay, displayed when a magic key is pressed.
  char** text_;
  size_t text_col_, text_row_;

  bool show_text;
  bool show_text_ever;  // has show_text ever been true?

  std::vector<std::string> title_lines_;

  bool scrollable_menu_;
  std::unique_ptr<Menu> menu_;

  // An alternate text screen, swapped with 'text_' when we're viewing a log file.
  char** file_viewer_text_;

  std::thread progress_thread_;
  std::atomic<bool> progress_thread_stopped_{ false };

  // Number of intro frames and loop frames in the animation.
  size_t intro_frames;
  size_t loop_frames;

  size_t current_frame;
  bool intro_done;

  int stage, max_stage;

  int char_width_;
  int char_height_;

  // The locale that's used to show the rendered texts.
  std::string locale_;
  bool rtl_locale_;

  std::mutex updateMutex;

 private:
  void SetLocale(const std::string&);

  // Display the background texts for "erasing", "error", "no_command" and "installing" for the
  // selected locale.
  void SelectAndShowBackgroundText(const std::vector<std::string>& locales_entries, size_t sel);
};

#endif  // RECOVERY_UI_H
