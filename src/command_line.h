/*
 * command_line.h - Command line parser and helpers
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SRC_COMMAND_LINE_H_
#define SRC_COMMAND_LINE_H_

#include <string>
#include <vector>
#include <functional>
#include <map>

class CommandLine {
 public:
  class Option {
   protected:
    typedef struct Parameter {
      std::string name;
      std::function<bool(std::istream&)> handler;
    } Parameter;
    typedef std::vector<Parameter> Parameters;

   protected:
    std::string comment;
    std::function<void()> handler;
    Parameters parameters;

   public:
    Option() {}
    explicit Option(const std::string &_comment,
                    std::function<void()> _handler = [](){})
      : comment(_comment)
      , handler(_handler) {
    }
    Option(const Option&) = default;

    bool has_parameter() const { return (parameters.size() > 0); }
    std::string get_comment() const { return comment; }
    void show_help();
    void show_usage();

    Option &add_parameter(const std::string &name,
                          std::function<bool(std::istream&)> handler);
    bool process(std::vector<std::string> params);
  };
  typedef std::map<char, Option> Options;

 protected:
  std::string path;
  std::string progname;
  std::string comment;
  Options options;

 public:
  CommandLine();

  bool process(int argc, char *argv[]);

  Option &add_option(char key, const std::string &comment,
                     std::function<void()> handler = [](){});

  std::string get_path() const { return path; }
  std::string get_progname() const { return progname; }
  void set_comment(const std::string &_comment) { comment = _comment; }

  void show_help();
  void show_usage();
};

#endif  // SRC_COMMAND_LINE_H_

