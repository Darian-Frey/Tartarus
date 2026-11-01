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

// Developer console logic (F-010). See console_core.h.

#include "console_core.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace tartarus {

namespace {

std::string Lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}

std::string Join(const CommandArgs &args, size_t from) {
  std::string out;
  for (size_t i = from; i < args.size(); ++i) {
    if (i > from)
      out += ' ';
    out += args[i];
  }
  return out;
}

constexpr int kMaxExecDepth = 8;

} // namespace

ConsoleCore::ConsoleCore(CvarRegistry &cvars) : cvars_(cvars) { RegisterBuiltins(); }

// ---------------------------------------------------------------- output

void ConsoleCore::Print(std::string_view text) {
  std::lock_guard<std::mutex> lock(pending_mutex_);
  size_t start = 0;
  while (true) {
    const size_t nl = text.find('\n', start);
    pending_.emplace_back(text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start));
    if (nl == std::string_view::npos)
      break;
    start = nl + 1;
    if (start == text.size())
      break; // trailing newline: no empty extra line
  }
}

void ConsoleCore::Pump() {
  std::vector<std::string> moved;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    moved.swap(pending_);
  }
  for (auto &line : moved) {
    AddLine(std::move(line));
  }
}

void ConsoleCore::AddLine(std::string line) {
  lines_.push_back(std::move(line));
  while (lines_.size() > max_lines_) {
    lines_.pop_front();
  }
  if (scroll_ > 0) {
    ++scroll_; // keep the view still while the user is scrolled back
  }
  scroll_ = std::min<int>(scroll_, static_cast<int>(lines_.size()) - 1);
  scroll_ = std::max(scroll_, 0);
}

void ConsoleCore::SetMaxLines(size_t n) {
  max_lines_ = std::max<size_t>(n, 1);
  while (lines_.size() > max_lines_)
    lines_.pop_front();
}

void ConsoleCore::Scroll(int lines) {
  scroll_ = std::clamp(scroll_ + lines, 0, std::max(0, static_cast<int>(lines_.size()) - 1));
}

// ---------------------------------------------------------------- commands

void ConsoleCore::AddCommand(ConsoleCommand cmd) {
  std::string key = Lower(cmd.name);
  commands_[key] = std::move(cmd);
}

const ConsoleCommand *ConsoleCore::FindCommand(std::string_view name) const {
  auto it = commands_.find(Lower(name));
  return it == commands_.end() ? nullptr : &it->second;
}

void ConsoleCore::Execute(std::string_view line, CvarSource source) {
  for (const CommandArgs &args : ParseCommandLine(line)) {
    Dispatch(args, source);
  }
  Pump();
}

void ConsoleCore::Dispatch(const CommandArgs &args, CvarSource source) {
  if (const ConsoleCommand *cmd = FindCommand(args[0])) {
    if ((cmd->flags & CMD_CHEAT) && !(cheat_allowed && cheat_allowed(*cmd))) {
      Print(cmd->name + ": cheat. Unlock it first (cheat menu or 'cheat <password>').");
      return;
    }
    cmd->run(args, *this);
    return;
  }
  if (Cvar *cv = cvars_.Find(args[0])) {
    if (args.size() == 1) {
      Print(cv->Describe());
      return;
    }
    const CvarSetResult r = cvars_.Set(cv->Name(), Join(args, 1), source);
    if (r == CvarSetResult::Ok) {
      Print(cv->Name() + " = " + QuoteArg(cv->StoredString()));
    } else if (CvarSetAccepted(r)) {
      Print(cv->Name() + " = " + QuoteArg(r == CvarSetResult::Latched ? cv->LatchedString().value_or("") : cv->StoredString()) +
            " (" + CvarSetResultText(r) + ")");
    } else {
      Print(cv->Name() + ": " + CvarSetResultText(r));
    }
    return;
  }
  Print("Unknown command or cvar: " + args[0] + " (try 'help' or 'find " + args[0] + "')");
}

bool ConsoleCore::ExecFile(const fs::path &file) {
  if (exec_depth_ >= kMaxExecDepth) {
    Print("exec: scripts nested too deeply (" + std::to_string(kMaxExecDepth) + ")");
    return false;
  }
  fs::path path = file;
  if (path.is_relative() && !exec_dir_.empty()) {
    path = exec_dir_ / path;
  }
  std::ifstream in(path);
  if (!in) {
    Print("exec: cannot open " + path.string());
    return false;
  }
  ++exec_depth_;
  std::string line;
  while (std::getline(in, line)) {
    for (const CommandArgs &args : ParseCommandLine(line)) {
      Dispatch(args, CvarSource::User);
    }
  }
  --exec_depth_;
  Pump();
  return true;
}

void ConsoleCore::RegisterBuiltins() {
  AddCommand({"help", "help [name]", "List commands, or describe a command or cvar", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) {
                if (a.size() > 1) {
                  if (const ConsoleCommand *cmd = c.FindCommand(a[1])) {
                    c.Print(cmd->usage + " - " + cmd->help + ((cmd->flags & CMD_CHEAT) ? " [cheat]" : ""));
                  } else if (Cvar *cv = c.cvars_.Find(a[1])) {
                    c.Print(cv->Describe());
                  } else {
                    c.Print("help: nothing called " + a[1]);
                  }
                  return;
                }
                c.Print("Commands (help <name> for details; cvarlist for settings):");
                for (const auto &[key, cmd] : c.commands_) {
                  c.Print("  " + cmd.usage + " - " + cmd.help + ((cmd.flags & CMD_CHEAT) ? " [cheat]" : ""));
                }
              }});
  AddCommand({"find", "find <substring>", "List commands and cvars whose name contains the text", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) {
                if (a.size() < 2) {
                  c.Print("usage: find <substring>");
                  return;
                }
                const std::string needle = Lower(a[1]);
                int n = 0;
                for (const auto &[key, cmd] : c.commands_) {
                  if (key.find(needle) != std::string::npos) {
                    c.Print("  " + cmd.usage + " - " + cmd.help);
                    ++n;
                  }
                }
                for (Cvar *cv : c.cvars_.List()) {
                  if (Lower(cv->Name()).find(needle) != std::string::npos) {
                    c.Print("  " + cv->Describe());
                    ++n;
                  }
                }
                c.Print(std::to_string(n) + " match" + (n == 1 ? "" : "es"));
              }});
  AddCommand({"cvarlist", "cvarlist [prefix]", "List cvars with values and flags", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) {
                const auto list = c.cvars_.List(a.size() > 1 ? a[1] : "");
                for (Cvar *cv : list) {
                  c.Print("  " + cv->Describe());
                }
                c.Print(std::to_string(list.size()) + " cvar" + (list.size() == 1 ? "" : "s"));
              }});
  AddCommand({"set", "set <cvar> <value>", "Set a cvar (same as '<cvar> <value>')", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) {
                if (a.size() < 3) {
                  c.Print("usage: set <cvar> <value>");
                  return;
                }
                CommandArgs rest(a.begin() + 1, a.end());
                c.Dispatch(rest, CvarSource::User);
              }});
  AddCommand({"reset", "reset <cvar>", "Return a cvar to its default", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) {
                if (a.size() < 2) {
                  c.Print("usage: reset <cvar>");
                  return;
                }
                if (Cvar *cv = c.cvars_.Find(a[1])) {
                  c.Dispatch({cv->Name(), cv->DefaultString()}, CvarSource::User);
                } else {
                  c.Print("reset: no cvar called " + a[1]);
                }
              }});
  AddCommand({"toggle", "toggle <cvar>", "Flip a bool cvar", CMD_NONE, [](const CommandArgs &a, ConsoleCore &c) {
                Cvar *cv = a.size() > 1 ? c.cvars_.Find(a[1]) : nullptr;
                if (cv == nullptr || cv->Type() != CvarType::Bool) {
                  c.Print("usage: toggle <bool cvar>");
                  return;
                }
                c.Dispatch({cv->Name(), cv->StoredString() == "1" ? "0" : "1"}, CvarSource::User);
              }});
  AddCommand({"echo", "echo <text>", "Print text", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) { c.Print(Join(a, 1)); }});
  AddCommand({"clear", "clear", "Clear the scrollback", CMD_NONE,
              [](const CommandArgs &, ConsoleCore &c) { c.Clear(); }});
  AddCommand({"exec", "exec <file>", "Run a script from the writable base directory", CMD_NONE,
              [](const CommandArgs &a, ConsoleCore &c) {
                if (a.size() < 2) {
                  c.Print("usage: exec <file>");
                  return;
                }
                c.ExecFile(a[1]);
              }});
}

// ---------------------------------------------------------------- input line

void ConsoleCore::InsertText(std::string_view text) {
  input_.insert(cursor_, text);
  cursor_ += text.size();
}

void ConsoleCore::Backspace() {
  if (cursor_ > 0) {
    input_.erase(--cursor_, 1);
  }
}

void ConsoleCore::DeleteForward() {
  if (cursor_ < input_.size()) {
    input_.erase(cursor_, 1);
  }
}

void ConsoleCore::MoveCursor(int delta) {
  const long long pos = static_cast<long long>(cursor_) + delta;
  cursor_ = static_cast<size_t>(std::clamp<long long>(pos, 0, static_cast<long long>(input_.size())));
}

void ConsoleCore::ClearInput() {
  input_.clear();
  cursor_ = 0;
  history_pos_ = history_.size();
}

void ConsoleCore::Submit() {
  const std::string line = input_;
  ClearInput();
  ScrollToBottom();
  Print("] " + line);
  Pump();
  if (line.find_first_not_of(" \t") == std::string::npos) {
    return;
  }
  if (history_.empty() || history_.back() != line) {
    history_.push_back(line);
    if (history_.size() > kMaxHistory) {
      history_.erase(history_.begin(), history_.begin() + (history_.size() - kMaxHistory));
    }
  }
  history_pos_ = history_.size();
  Execute(line);
}

// ---------------------------------------------------------------- history

void ConsoleCore::SetHistory(std::vector<std::string> history) {
  history_ = std::move(history);
  if (history_.size() > kMaxHistory) {
    history_.erase(history_.begin(), history_.begin() + (history_.size() - kMaxHistory));
  }
  history_pos_ = history_.size();
}

void ConsoleCore::HistoryPrev() {
  if (history_.empty() || history_pos_ == 0)
    return;
  if (history_pos_ == history_.size())
    draft_ = input_;
  input_ = history_[--history_pos_];
  cursor_ = input_.size();
}

void ConsoleCore::HistoryNext() {
  if (history_pos_ >= history_.size())
    return;
  ++history_pos_;
  input_ = (history_pos_ == history_.size()) ? draft_ : history_[history_pos_];
  cursor_ = input_.size();
}

// ---------------------------------------------------------------- completion

std::vector<std::string> ConsoleCore::Candidates(std::string_view prefix) const {
  const std::string p = Lower(prefix);
  std::set<std::string> out;
  for (const auto &[key, cmd] : commands_) {
    if (key.compare(0, p.size(), p) == 0)
      out.insert(cmd.name);
  }
  for (Cvar *cv : cvars_.List()) {
    if (Lower(cv->Name()).compare(0, p.size(), p) == 0)
      out.insert(cv->Name());
  }
  return {out.begin(), out.end()};
}

void ConsoleCore::Complete() {
  // Completes the command word, or the name after help/set/reset/toggle.
  size_t word_start = cursor_;
  while (word_start > 0 && !std::isspace(static_cast<unsigned char>(input_[word_start - 1])) &&
         input_[word_start - 1] != ';')
    --word_start;
  size_t cmd_start = word_start;
  while (cmd_start > 0 && input_[cmd_start - 1] != ';')
    --cmd_start;

  const auto before = ParseCommandLine(input_.substr(cmd_start, word_start - cmd_start));
  const size_t words_before = before.empty() ? 0 : before[0].size();
  if (words_before == 1) {
    const std::string first = Lower(before[0][0]);
    if (first != "help" && first != "set" && first != "reset" && first != "toggle")
      return;
  } else if (words_before > 1) {
    return;
  }

  const std::string prefix = input_.substr(word_start, cursor_ - word_start);
  const auto cands = Candidates(prefix);
  if (cands.empty())
    return;
  std::string completion;
  if (cands.size() == 1) {
    completion = cands[0] + " ";
  } else {
    completion = cands[0];
    for (const auto &c : cands) {
      size_t n = 0;
      while (n < completion.size() && n < c.size() &&
             std::tolower(static_cast<unsigned char>(completion[n])) == std::tolower(static_cast<unsigned char>(c[n])))
        ++n;
      completion.resize(n);
    }
    Print("] " + input_);
    std::string row;
    for (const auto &c : cands) {
      if (row.size() + c.size() > 72) {
        Print("  " + row);
        row.clear();
      }
      row += c + "  ";
    }
    if (!row.empty())
      Print("  " + row);
    Pump();
  }
  if (completion.size() >= prefix.size()) {
    input_.replace(word_start, cursor_ - word_start, completion);
    cursor_ = word_start + completion.size();
  }
}

} // namespace tartarus
