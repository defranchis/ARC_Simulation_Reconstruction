// Martin Duy Tat 5th May 2022

#include<string>
#include<unordered_map>
#include<stdexcept>
#include<utility>
#include<fstream>
#include<sstream>
#include<vector>
#include<sstream>
#include<algorithm>
#include<cctype>
#include"Settings.h"

std::unordered_map<std::string, ssMap> Settings::m_Settings;

namespace {
  /**
   * Error message naming the setting that could not be converted
   */
  std::string ParseError(const std::string &Setting,
                         const std::string &Value,
                         const std::string &Type) {
    const std::size_t SlashPos = Setting.find('/');
    return "Cannot parse value '" + Value + "' of "
           + Setting.substr(SlashPos + 1) + " in settings "
           + Setting.substr(0, SlashPos) + " as " + Type;
  }
  /**
   * Strip all whitespace, so that a comma separated list may be spaced out
   */
  std::string RemoveWhitespace(std::string Text) {
    Text.erase(std::remove_if(Text.begin(), Text.end(),
                              [] (unsigned char Character) {
                                return std::isspace(Character) != 0;
                              }),
               Text.end());
    return Text;
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
  const std::string Value = GetString(Setting);
  std::size_t CharactersUsed = 0;
  const double Number = std::stod(Value, &CharactersUsed);
  if(CharactersUsed != Value.size()) {
    throw std::runtime_error(ParseError(Setting, Value, "number"));
  }
  return Number;
}

int Settings::GetInt(const std::string &Setting) {
  const std::string Value = GetString(Setting);
  std::size_t CharactersUsed = 0;
  const int Integer = std::stoi(Value, &CharactersUsed);
  if(CharactersUsed != Value.size()) {
    throw std::runtime_error(ParseError(Setting, Value, "integer"));
  }
  return Integer;
}

std::size_t Settings::GetSizeT(const std::string &Setting) {
  const std::string Value = GetString(Setting);
  const int Integer = GetInt(Setting);
  if(Integer < 0 || Value.rfind('-', 0) == 0) {
    throw std::runtime_error("Cannot load negative integer into std::size_t");
  } else {
    return static_cast<std::size_t>(Integer);
  }
}

bool Settings::GetBool(const std::string &Setting) {
  return GetString(Setting) == "true";
}

std::vector<int> Settings::GetIntVector(const std::string &Setting) {
  std::string CommaSeparatedList = RemoveWhitespace(GetRawString(Setting));
  std::replace(CommaSeparatedList.begin(), CommaSeparatedList.end(), ',', ' ');
  std::stringstream ss(CommaSeparatedList);
  std::vector<int> List;
  int Number;
  while(ss >> Number) {
    List.push_back(Number);
  }
  return List;
}

std::vector<std::size_t> Settings::GetSizeTVector(const std::string &Setting) {
  std::string CommaSeparatedList = RemoveWhitespace(GetRawString(Setting));
  std::replace(CommaSeparatedList.begin(), CommaSeparatedList.end(), ',', ' ');
  std::stringstream ss(CommaSeparatedList);
  std::vector<std::size_t> List;
  std::size_t Number;
  while(ss >> Number) {
    List.push_back(Number);
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
