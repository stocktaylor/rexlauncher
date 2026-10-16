// rexlauncher - settings persistence and the settings restart.

#include "settings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string_view>

#include <rex/cvar.h>
#include <rex/logging.h>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace rexlauncher {

namespace {

std::string TomlValue(const std::string& cvar) {
  std::string value = rex::cvar::GetFlagByName(cvar);
  const auto* info = rex::cvar::GetFlagInfo(cvar);
  if (info && info->type != rex::cvar::FlagType::String) {
    return value;
  }
  std::string quoted = "\"";
  for (char ch : value) {
    if (ch == '"' || ch == '\\') {
      quoted += '\\';
    }
    quoted += ch;
  }
  return quoted + "\"";
}

}  // namespace

bool IsLockedByCommandLine(const std::string& cvar) {
  return rex::cvar::GetFlagSource(cvar) == rex::cvar::Source::kCommandLine;
}

void SaveToConfig(const std::filesystem::path& path, const std::vector<std::string>& cvars) {
  if (path.empty()) {
    return;
  }
  std::set<std::string> keys;
  std::vector<std::string> ours;
  for (const auto& cvar : cvars) {
    if (rex::cvar::GetFlagInfo(cvar) && !IsLockedByCommandLine(cvar) && keys.insert(cvar).second) {
      ours.push_back(cvar + " = " + TomlValue(cvar));
    }
  }

  std::vector<std::string> head, tail;
  bool in_tables = false;
  if (std::ifstream in(path); in) {
    for (std::string line; std::getline(in, line);) {
      std::string_view trimmed = line;
      trimmed.remove_prefix(std::min(trimmed.find_first_not_of(" \t"), trimmed.size()));
      in_tables = in_tables || trimmed.starts_with("[");
      if (in_tables) {
        tail.push_back(line);
        continue;
      }
      auto eq = trimmed.find('=');
      if (eq != std::string_view::npos) {
        std::string_view key = trimmed.substr(0, eq);
        key.remove_suffix(key.size() - std::min(key.find_last_not_of(" \t") + 1, key.size()));
        if (keys.count(std::string(key))) {
          continue;  // replaced below
        }
      }
      if (trimmed != "# rexlauncher") {
        head.push_back(line);
      }
    }
  }

  std::ostringstream out;
  for (const auto& line : head) {
    out << line << "\n";
  }
  out << "# rexlauncher\n";
  for (const auto& line : ours) {
    out << line << "\n";
  }
  for (const auto& line : tail) {
    out << line << "\n";
  }
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream file(path, std::ios::trunc);
  if (!file) {
    REXLOG_WARN("rexlauncher: could not write {}", path.string());
    return;
  }
  file << out.str();
}

void RestartProcess() {
#if defined(__linux__)
  std::ifstream in("/proc/self/cmdline", std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::vector<std::string> args;
  for (size_t start = 0; start < data.size();) {
    size_t end = data.find('\0', start);
    if (end == std::string::npos) {
      end = data.size();
    }
    args.emplace_back(data.substr(start, end - start));
    start = end + 1;
  }
  if (args.empty()) {
    return;
  }
  std::vector<char*> argv;
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  setenv(kAutostartEnv, "1", 1);
  REXLOG_INFO("rexlauncher: restarting to apply settings");
  execv("/proc/self/exe", argv.data());
  REXLOG_WARN("rexlauncher: restart failed; settings apply at the next start");
  unsetenv(kAutostartEnv);
#endif
}

}  // namespace rexlauncher
