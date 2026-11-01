/*
 * Tartarus, a fork of Descent 3
 * Copyright (C) 2026 Tartarus contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Developer console logic (F-010, SPEC.md §3): scrollback, line editing,
// history, tab completion and command dispatch. Pure standard C++ (no engine
// headers) so it can be unit-tested; console.cpp adds keys, drawing, the plog
// mirror and engine commands.

#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "cmdparse.h"
#include "cvar.h"

namespace tartarus {

class ConsoleCore;

enum ConsoleCommandFlags : uint32_t {
  CMD_NONE = 0,
  CMD_CHEAT = 1u << 0, // refused unless the cheat policy allows it (F-012 / Debug build)
};

struct ConsoleCommand {
  std::string name;
  std::string usage; // e.g. "find <substring>"
  std::string help;  // one line
  uint32_t flags = CMD_NONE;
  std::function<void(const CommandArgs &args, ConsoleCore &con)> run;
};

class ConsoleCore {
public:
  explicit ConsoleCore(CvarRegistry &cvars);

  // ---- Output (Print is thread-safe; plog may call it from any thread) ----
  void Print(std::string_view text); // splits on '\n'
  /// Moves printed text into the scrollback. Call from the game thread before reading Lines().
  void Pump();
  const std::deque<std::string> &Lines() const { return lines_; }
  void SetMaxLines(size_t n);
  void Clear() { lines_.clear(); scroll_ = 0; }

  // ---- Commands ----
  void AddCommand(ConsoleCommand cmd);
  const ConsoleCommand *FindCommand(std::string_view name) const;
  /// Runs a line (';'-separated commands). `source` decides how cvar sets are policed.
  void Execute(std::string_view line, CvarSource source = CvarSource::User);
  /// Runs a script; relative names resolve against the exec directory.
  bool ExecFile(const std::filesystem::path &file);
  void SetExecDirectory(std::filesystem::path dir) { exec_dir_ = std::move(dir); }
  std::function<bool(const ConsoleCommand &)> cheat_allowed; // unset = refuse CMD_CHEAT

  // ---- Input line ----
  const std::string &Input() const { return input_; }
  size_t Cursor() const { return cursor_; }
  void InsertText(std::string_view text);
  void Backspace();
  void DeleteForward();
  void MoveCursor(int delta);
  void CursorHome() { cursor_ = 0; }
  void CursorEnd() { cursor_ = input_.size(); }
  void ClearInput();
  /// Echoes "] line" into the scrollback, records history, executes.
  void Submit();

  // ---- History ----
  void HistoryPrev();
  void HistoryNext();
  const std::vector<std::string> &History() const { return history_; }
  void SetHistory(std::vector<std::string> history);
  static constexpr size_t kMaxHistory = 256;

  // ---- Completion: commands and cvars, on the word under construction ----
  /// Unique match: completes it (plus a space). Several: extends to the longest
  /// common prefix and prints the candidates.
  void Complete();
  std::vector<std::string> Candidates(std::string_view prefix) const;

  // ---- Scrolling (0 = newest line at the bottom) ----
  int ScrollOffset() const { return scroll_; }
  void Scroll(int lines);
  void ScrollToBottom() { scroll_ = 0; }

private:
  void RegisterBuiltins();
  void Dispatch(const CommandArgs &args, CvarSource source);
  void AddLine(std::string line);

  CvarRegistry &cvars_;
  std::map<std::string, ConsoleCommand, std::less<>> commands_;

  std::mutex pending_mutex_;
  std::vector<std::string> pending_;
  std::deque<std::string> lines_;
  size_t max_lines_ = 2000;
  int scroll_ = 0;

  std::string input_;
  size_t cursor_ = 0;

  std::vector<std::string> history_;
  size_t history_pos_ = 0; // == history_.size() when editing a fresh line
  std::string draft_;      // the fresh line saved while browsing history

  std::filesystem::path exec_dir_;
  int exec_depth_ = 0;
};

} // namespace tartarus
