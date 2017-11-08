/*
 * command_line.cc - Command line parser and helpers
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

#include "src/command_line.h"

#include <queue>
#include <iostream>
#include <sstream>

CommandLine::CommandLine() {
}

bool
CommandLine::process(int argc, char *argv[]) {
  std::queue<std::string> args;

  for (int i = 0; i < argc; i++) {
    args.push(argv[i]);
  }

  if (args.size() < 1) {
    return false;
  }

  path = args.front();
  args.pop();
  size_t pos = path.find_last_of("\\/");
  if (pos != std::string::npos) {
    progname = path.substr(pos + 1, path.length() - pos - 1);
  } else {
    progname = path;
  }

  while (args.size()) {
    std::string arg = args.front();
    args.pop();
    if ((arg.length() != 2) || (arg[0] != '-')) {
      std::cout << "Unknown command line parameter: \"" << arg << "\"\n";
      continue;
    }

    char key = arg[1];
    Options::iterator it = options.find(key);
    if (it == options.end()) {
      show_help();
      return false;
    }
    Option &opt = options[key];
    std::vector<std::string> params;
    while (args.size() && (args.front()[0] != '-')) {
      params.push_back(args.front());
      args.pop();
    }
    if (!opt.process(params)) {
      show_help();
      return false;
    }
  }

  return true;
}

CommandLine::Option &
CommandLine::add_option(char key, const std::string &comment,
                        std::function<void()> handler) {
  options[key] = Option(comment, handler);
  return options[key];
}

void
CommandLine::show_help() {
  show_usage();
  std::cout << "\n";
  for (auto &opt : options) {
    std::cout << "\t-" << opt.first;
    opt.second.show_help();
    std::cout << "\n";
  }

  if (!comment.empty()) {
    std::cout << "\n" << comment << "\n";
  }
}

void
CommandLine::show_usage() {
  std::cout << "Usage: " << path;
  for (auto &opt : options) {
    std::cout << " [-" << opt.first;
    opt.second.show_usage();
    std::cout << "]";
  }
  std::cout << "\n";
}

CommandLine::Option &
CommandLine::Option::add_parameter(const std::string &name,
                                  std::function<bool(std::istream&)> _handler) {
  Parameter parameter;
  parameter.name = name;
  parameter.handler = _handler;
  parameters.push_back(parameter);
  return *this;
}

bool
CommandLine::Option::process(std::vector<std::string> params) {
  handler();
  if (params.size() != parameters.size()) {
    return false;
  }
  for (size_t i = 0; i < params.size(); i++) {
    std::stringstream s;
    s << params[i];
    if (!parameters[i].handler(s)) {
      return false;
    }
  }
  return true;
}

void
CommandLine::Option::show_help() {
  if (parameters.size() > 0) {
    for (Parameter &par : parameters) {
      std::cout << " " << par.name << "\t";
    }
  } else {
    std::cout << "\t\t";
  }
  std::cout << comment;
}

void
CommandLine::Option::show_usage() {
  for (Parameter &par : parameters) {
    std::cout << " " << par.name;
  }
}
