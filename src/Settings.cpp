// Martin Duy Tat 5th May 2022

#include<string>
#include<unordered_map>
#include<stdexcept>
#include<utility>
#include<fstream>
#include<sstream>
#include<vector>
#include<sstream>
#include"Settings.h"

std::unordered_map<std::string, ssMap> Settings::m_Settings;

namespace {
  /**
   * Name of the setting, for error messages
   */
  std::string SettingName(const std::string &Setting) {
    const std::size_t SlashPos = Setting.find('/');
    return Setting.substr(SlashPos + 1) + " in settings "
           + Setting.substr(0, SlashPos);
  }
  /**
   * Error message naming the setting that could not be converted
   */
  std::string ParseError(const std::string &Setting,
                         const std::string &Value,
                         const std::string &Type) {
    return "Cannot parse value '" + Value + "' of " + SettingName(Setting)
           + " as " + Type;
  }
  /**
   * Convert a value, which has to be consumed in full
   */
  double ParseDouble(const std::string &Setting, const std::string &Value) {
    std::size_t CharactersUsed = 0;
    const double Number = std::stod(Value, &CharactersUsed);
    if(CharactersUsed != Value.size()) {
      throw std::runtime_error(ParseError(Setting, Value, "number"));
    }
    return Number;
  }
  int ParseIntValue(const std::string &Setting, const std::string &Value) {
    std::size_t CharactersUsed = 0;
    const int Integer = std::stoi(Value, &CharactersUsed);
    if(CharactersUsed != Value.size()) {
      throw std::runtime_error(ParseError(Setting, Value, "integer"));
    }
    return Integer;
  }
  std::size_t ParseSizeT(const std::string &Setting, const std::string &Value) {
    const int Integer = ParseIntValue(Setting, Value);
    if(Integer < 0 || Value.rfind('-', 0) == 0) {
      throw std::runtime_error("Cannot load negative value '" + Value + "' of "
                               + SettingName(Setting) + " into std::size_t");
    }
    return static_cast<std::size_t>(Integer);
  }
  /**
   * Remove leading and trailing whitespace
   */
  std::string Trim(const std::string &Text) {
    const std::size_t First = Text.find_first_not_of(" \t");
    if(First == std::string::npos) {
      return {};
    }
    return Text.substr(First, Text.find_last_not_of(" \t") - First + 1);
  }
  /**
   * Split a comma separated list, one trimmed entry per comma
   * Entries may be spaced out, but an entry may not contain whitespace itself
   */
  std::vector<std::string> SplitList(const std::string &Setting,
                                     const std::string &Line) {
    std::vector<std::string> Entries;
    std::size_t Start = 0;
    while(true) {
      const std::size_t Comma = Line.find(',', Start);
      const std::size_t Length =
        Comma == std::string::npos ? std::string::npos : Comma - Start;
      const std::string Entry = Trim(Line.substr(Start, Length));
      if(Entry.empty()) {
        throw std::runtime_error("Empty entry in the list "
                                 + SettingName(Setting));
      }
      if(Entry.find_first_of(" \t") != std::string::npos) {
        throw std::runtime_error("Entry '" + Entry + "' of the list "
                                 + SettingName(Setting)
                                 + " contains whitespace, separate the entries"
                                 + " with commas");
      }
      Entries.push_back(Entry);
      if(Comma == std::string::npos) {
        return Entries;
      }
      Start = Comma + 1;
    }
  }
}

void Settings::AddSettings(const std::string &Name, const std::string &Filename) {
  if(m_Settings.find(Name) != m_Settings.end()) {
    throw std::runtime_error("Settings " + Name + " already exists");
  }
  ssMap NewSettings;
  std::ifstream File(Filename);
  if(!File.is_open()) {
    throw std::runtime_error("Cannot open settings file " + Filename
			     + " for settings " + Name);
  }
  std::string Line;
  std::size_t LineNumber = 0;
  while(std::getline(File, Line)) {
    LineNumber++;
    Line = Line.substr(0, Line.find('#'));
    std::stringstream ss(Line);
    std::string Key;
    if(!(ss >> Key)) {
      // Blank line, or a line that is only a comment
      continue;
    }
    // Store the rest of the line, since a value may be a list with whitespace
    std::string Value;
    std::getline(ss >> std::ws, Value);
    if(Value.empty()) {
      throw std::runtime_error("Key " + Key + " on line "
                               + std::to_string(LineNumber) + " of settings file "
                               + Filename + " has no value");
    }
    if(NewSettings.find(Key) != NewSettings.end()) {
      throw std::runtime_error("Key " + Key + " in settings " + Name + " already exists");
    }
    NewSettings.insert({Key, Value});
  }
  File.close();
  m_Settings.insert({Name, std::move(NewSettings)});
}

std::string Settings::GetString(const std::string &Setting) {
  // Only the first token is the value, the rest of the line may be a comment
  const std::string Value = GetRawString(Setting);
  return Value.substr(0, Value.find_first_of(" \t"));
}

std::string Settings::GetRawString(const std::string &Setting) {
  const std::size_t SlashPos = Setting.find('/');
  std::string Name = Setting.substr(0, SlashPos);
  auto iter1 = m_Settings.find(Name);
  if(iter1 == m_Settings.end()) {
    std::string Known;
    for(const auto &[KnownName, KnownSettings] : m_Settings) {
      Known += " " + KnownName;
    }
    throw std::runtime_error("Cannot find settings name " + Name
			     + ", the settings added are:" + Known);
  }
  std::string Key = Setting.substr(SlashPos + 1);
  auto iter2 = iter1->second.find(Key);
  if(iter2 == iter1->second.end()) {
    throw std::runtime_error("Cannot find key " + Key + " in settings " + Name);
  }
  return iter2->second;
}

double Settings::GetDouble(const std::string &Setting) {
  return ParseDouble(Setting, GetString(Setting));
}

int Settings::GetInt(const std::string &Setting) {
  return ParseIntValue(Setting, GetString(Setting));
}

std::size_t Settings::GetSizeT(const std::string &Setting) {
  return ParseSizeT(Setting, GetString(Setting));
}

bool Settings::GetBool(const std::string &Setting) {
  const std::string Value = GetString(Setting);
  if(Value == "true") {
    return true;
  } else if(Value == "false") {
    return false;
  } else {
    throw std::runtime_error(
      ParseError(Setting, Value, "boolean, use true or false"));
  }
}

std::vector<int> Settings::GetIntVector(const std::string &Setting) {
  std::vector<int> List;
  for(const auto &Entry : SplitList(Setting, GetRawString(Setting))) {
    List.push_back(ParseIntValue(Setting, Entry));
  }
  return List;
}

std::vector<std::size_t> Settings::GetSizeTVector(const std::string &Setting) {
  std::vector<std::size_t> List;
  for(const auto &Entry : SplitList(Setting, GetRawString(Setting))) {
    List.push_back(ParseSizeT(Setting, Entry));
  }
  return List;
}

bool Settings::Exists(const std::string &Setting) {
  // Same lookup as GetString, without throwing: this is called per photon
  const std::size_t SlashPos = Setting.find('/');
  auto iter1 = m_Settings.find(Setting.substr(0, SlashPos));
  if(iter1 == m_Settings.end()) {
    return false;
  }
  return iter1->second.find(Setting.substr(SlashPos + 1)) != iter1->second.end();
}

int Settings::ParseInt(const std::string &Setting, const std::string &Value) {
  return ParseIntValue(Setting, Value);
}
