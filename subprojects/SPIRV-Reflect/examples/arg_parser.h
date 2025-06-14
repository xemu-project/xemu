#ifndef __VERIFLECT_ARG_PARSER_H__
#define __VERIFLECT_ARG_PARSER_H__

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class ArgParser {
 public:
  enum OptionType { OPTION_TYPE_UNDEFINED = 0, OPTION_TYPE_FLAG, OPTION_TYPE_STRING, OPTION_TYPE_INT, OPTION_TYPE_FLOAT };

  struct OptionValue {
    std::string str;
    union {
      int i32;
      float f32;
    };
  };

  struct Option {
    std::string short_name;
    std::string long_name;
    OptionType type;
    std::string desc;
    OptionValue value;
    OptionValue default_value;
    bool parsed;
  };

  ArgParser();
  ~ArgParser();

  bool AddFlag(const std::string& short_name, const std::string& long_name, const std::string& desc);
  bool AddOptionString(const std::string& short_name, const std::string& long_name, const std::string& desc,
                       const std::string& default_value = "");
  bool AddOptionInt(const std::string& short_name, const std::string& long_name, const std::string& desc, int default_value = 0);
  bool AddOptionFloat(const std::string& short_name, const std::string& long_name, const std::string& desc,
                      float default_value = 0);

  bool Parse(int argc, char** argv, std::ostream& os);

  size_t GetArgCount() const;
  bool GetArg(size_t i, std::string* p_value) const;
  const std::vector<std::string>& GetArgs() const;

  bool GetFlag(const std::string& short_name, const std::string& long_name) const;
  bool GetString(const std::string& short_name, const std::string& long_name, std::string* p_value) const;
  bool GetInt(const std::string& short_name, const std::string& long_name, int* p_value) const;
  bool GetFloat(const std::string& short_name, const std::string& long_name, float* p_value) const;

  void PrintHelp(std::ostream& os);

 private:
  ArgParser::Option* FindOptionByShortName(const std::string& short_name);
  const ArgParser::Option* FindOptionByShortName(const std::string& short_name) const;
  ArgParser::Option* FindOptionByLongName(const std::string& long_name);
  const ArgParser::Option* FindOptionByLongName(const std::string& long_name) const;

 private:
  std::vector<ArgParser::Option> m_options;
  std::vector<std::string> m_args;
};

#endif  // __VERIFLECT_ARG_PARSER_H__