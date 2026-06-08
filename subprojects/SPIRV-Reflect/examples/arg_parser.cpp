#include "arg_parser.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

ArgParser::ArgParser() {}

ArgParser::~ArgParser() {}

ArgParser::Option* ArgParser::FindOptionByShortName(const std::string& short_name) {
  ArgParser::Option* p_option = nullptr;
  auto it = std::find_if(std::begin(m_options), std::end(m_options),
                         [short_name](const ArgParser::Option& elem) -> bool { return elem.short_name == short_name; });
  if (it != std::end(m_options)) {
    p_option = &(*it);
  }
  return p_option;
}

const ArgParser::Option* ArgParser::FindOptionByShortName(const std::string& short_name) const {
  const ArgParser::Option* p_option = nullptr;
  auto it = std::find_if(std::begin(m_options), std::end(m_options),
                         [short_name](const ArgParser::Option& elem) -> bool { return elem.short_name == short_name; });
  if (it != std::end(m_options)) {
    p_option = &(*it);
  }
  return p_option;
}

ArgParser::Option* ArgParser::FindOptionByLongName(const std::string& long_name) {
  ArgParser::Option* p_option = nullptr;
  auto it = std::find_if(std::begin(m_options), std::end(m_options),
                         [long_name](const ArgParser::Option& elem) -> bool { return elem.long_name == long_name; });
  if (it != std::end(m_options)) {
    p_option = &(*it);
  }
  return p_option;
}

const ArgParser::Option* ArgParser::FindOptionByLongName(const std::string& long_name) const {
  const ArgParser::Option* p_option = nullptr;
  auto it = std::find_if(std::begin(m_options), std::end(m_options),
                         [long_name](const ArgParser::Option& elem) -> bool { return elem.long_name == long_name; });
  if (it != std::end(m_options)) {
    p_option = &(*it);
  }
  return p_option;
}

bool ArgParser::AddFlag(const std::string& short_name, const std::string& long_name, const std::string& desc) {
  Option option = {};
  option.short_name = short_name;
  option.long_name = long_name;
  option.type = OPTION_TYPE_FLAG;
  option.desc = desc;
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);
  if ((p_short != nullptr) || (p_long != nullptr)) {
    return false;
  }
  m_options.push_back(option);
  return true;
}

bool ArgParser::AddOptionString(const std::string& short_name, const std::string& long_name, const std::string& desc,
                                const std::string& default_value) {
  Option option = {};
  option.short_name = short_name;
  option.long_name = long_name;
  option.type = OPTION_TYPE_STRING;
  option.desc = desc;
  option.default_value.str = default_value;
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);
  if ((p_short != nullptr) || (p_long != nullptr)) {
    return false;
  }
  m_options.push_back(option);
  return true;
}

bool ArgParser::AddOptionInt(const std::string& short_name, const std::string& long_name, const std::string& desc,
                             int default_value) {
  Option option = {};
  option.short_name = short_name;
  option.long_name = long_name;
  option.type = OPTION_TYPE_INT;
  option.desc = desc;
  option.default_value.i32 = default_value;
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);
  if ((p_short != nullptr) || (p_long != nullptr)) {
    return false;
  }
  m_options.push_back(option);
  return true;
}

bool ArgParser::AddOptionFloat(const std::string& short_name, const std::string& long_name, const std::string& desc,
                               float default_value) {
  Option option = {};
  option.short_name = short_name;
  option.long_name = long_name;
  option.type = OPTION_TYPE_FLOAT;
  option.desc = desc;
  option.default_value.f32 = default_value;
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);
  if ((p_short != nullptr) || (p_long != nullptr)) {
    return false;
  }
  m_options.push_back(option);
  return true;
}

bool ArgParser::Parse(int argc, char** argv, std::ostream& os) {
  for (auto& opt : m_options) {
    opt.value = opt.default_value;
    opt.parsed = false;
  }

  int i = 1;
  while (i < argc) {
    std::string s = argv[i];
    if (s[0] == '-') {
      ArgParser::Option* p_option = nullptr;
      if ((s.length() >= 2) && ((s[0] == '-') && (s[1] == '-'))) {
        std::string long_name = s.substr(2);
        p_option = FindOptionByLongName(long_name);
      } else {
        std::string short_name = s.substr(1);
        p_option = FindOptionByShortName(short_name);
      }

      if (p_option == nullptr) {
        os << "ERROR: invalid argument " << s << std::endl;
        return false;
      }

      switch (p_option->type) {
        case OPTION_TYPE_FLAG: {
          p_option->parsed = true;
          i += 1;
        } break;

        case OPTION_TYPE_STRING: {
          if ((i + 1) >= argc) {
            os << "ERROR: missing option data for " << s << std::endl;
            return false;
          }

          s = argv[i + 1];
          p_option->value.str = s;
          p_option->parsed = true;

          i += 2;
        } break;

        case OPTION_TYPE_INT: {
          if ((i + 1) >= argc) {
            os << "ERROR: missing option data for " << s << std::endl;
            return false;
          }

          s = argv[i + 1];
          p_option->value.i32 = atoi(s.c_str());
          p_option->parsed = true;

          i += 2;
        } break;

        case OPTION_TYPE_FLOAT: {
          if ((i + 1) >= argc) {
            os << "ERROR: missing option data for " << s << std::endl;
            return false;
          }

          s = argv[i + 1];
          p_option->value.f32 = static_cast<float>(atof(s.c_str()));
          p_option->parsed = true;

          i += 2;
        } break;

        case OPTION_TYPE_UNDEFINED: {
        } break;
      }
    } else {
      m_args.push_back(s);
      i += 1;
    }
  }

  return true;
}

size_t ArgParser::GetArgCount() const { return m_args.size(); }

bool ArgParser::GetArg(size_t i, std::string* p_value) const {
  if ((GetArgCount() == 0) && (i >= GetArgCount())) {
    return false;
  }

  if (p_value != nullptr) {
    *p_value = m_args[i];
  }

  return true;
}

const std::vector<std::string>& ArgParser::GetArgs() const { return m_args; }

bool ArgParser::GetFlag(const std::string& short_name, const std::string& long_name) const {
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);

  const ArgParser::Option* p_option = nullptr;
  if (p_short != nullptr) {
    p_option = p_short;
  }
  if ((p_option == nullptr) && (p_long != nullptr)) {
    p_option = p_short;
  }

  if (p_option == nullptr) {
    return false;
  }

  if (p_option->type != OPTION_TYPE_FLAG) {
    return false;
  }

  return p_option->parsed;
}

bool ArgParser::GetString(const std::string& short_name, const std::string& long_name, std::string* p_value) const {
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);

  const ArgParser::Option* p_option = nullptr;
  if (p_short != nullptr) {
    p_option = p_short;
  }
  if ((p_option == nullptr) && (p_long != nullptr)) {
    p_option = p_short;
  }

  if (p_option == nullptr) {
    return false;
  }

  if (!p_option->parsed || (p_option->type != OPTION_TYPE_STRING)) {
    return false;
  }

  if (p_value != nullptr) {
    *p_value = p_option->value.str;
  }

  return true;
}

bool ArgParser::GetInt(const std::string& short_name, const std::string& long_name, int* p_value) const {
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);

  const ArgParser::Option* p_option = nullptr;
  if (p_short != nullptr) {
    p_option = p_short;
  }
  if ((p_option == nullptr) && (p_long != nullptr)) {
    p_option = p_short;
  }

  if (p_option == nullptr) {
    return false;
  }

  if (!p_option->parsed || (p_option->type != OPTION_TYPE_INT)) {
    return false;
  }

  if (p_value != nullptr) {
    *p_value = p_option->value.i32;
  }

  return true;
}

bool ArgParser::GetFloat(const std::string& short_name, const std::string& long_name, float* p_value) const {
  auto p_short = FindOptionByShortName(short_name);
  auto p_long = FindOptionByLongName(long_name);

  const ArgParser::Option* p_option = nullptr;
  if (p_short != nullptr) {
    p_option = p_short;
  }
  if ((p_option == nullptr) && (p_long != nullptr)) {
    p_option = p_short;
  }

  if (p_option == nullptr) {
    return false;
  }

  if (!p_option->parsed || (p_option->type != OPTION_TYPE_FLOAT)) {
    return false;
  }

  if (p_value != nullptr) {
    *p_value = p_option->value.f32;
  }

  return true;
}

void ArgParser::PrintHelp(std::ostream& os) {
  (void)os;

  /*
    if (m_options.empty()) {
      return;
    }

    struct TextLine {
      std::string   option;
      std::string   desc;
    };
    std::vector<TextLine> text_lines;

    size_t max_width = 0;
    for (auto& it : m_options) {
      std::stringstream ss;
      ss << "--" << it.first;
      switch (it.second.type) {
        default: break;
        case OPTION_TYPE_STRING : ss << " " << "[s]"; break;
        case OPTION_TYPE_INT    : ss << " " << "[i]"; break;
        case OPTION_TYPE_FLOAT  : ss << " " << "[f]"; break;
      }

      std::string option = ss.str();
      max_width = std::max(max_width, option.size());

      TextLine tl;
      tl.option = option;
      tl.desc = it.second.desc;
      text_lines.push_back(tl);
    }
    max_width += 2;

    os << "\n";
    os << "Options:" << "\n";
    for (auto& tl : text_lines) {
      os << "  ";
      os << std::left << std::setw(max_width) << tl.option;
      os << tl.desc;
      os << "\n";
    }
  */
}
