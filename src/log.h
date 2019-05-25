/*
 * log.h - Logging
 *
 * Copyright (C) 2012-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

/* Log levels determine the importance of a log message. Use the
   following guidelines when deciding which level to use.

   - ERROR: The _user_ must take action to continue usage.
   Must usually be followed by exit(..) or abort().
   - WARN: Warn the _user_ of an important problem that the
   user might be able to resolve.
   - INFO: Information that is of general interest to _users_.
   - DEBUG: Log a problem that a _developer_ should look
   into and fix.
   - VERBOSE: Log any other information that a _developer_ might
   be interested in. */

#ifndef SRC_LOG_H_
#define SRC_LOG_H_

#include <ostream>
#include <string>

class Log {
 public:
  /* Log levels */
  typedef enum Level {
    LevelVerbose = 0,
    LevelDebug,
    LevelInfo,
    LevelWarn,
    LevelError,

    LevelMax
  } Level;

  class Stream {
   protected:
    std::ostream *stream;

   public:
    explicit Stream(std::ostream *_stream) : stream(_stream) {}
    ~Stream() {
      *stream << std::endl;
      stream->flush();
    }

    std::ostream *get_stream() { return stream; }

    template <class T> Stream & operator << (const T &val) {
      *stream << val;
      return *this;
    }

    Stream & operator << (const char val[]) {
      *stream << std::string(val);
      return *this;
    }
  };

  class Logger {
   protected:
    Level level;
    std::string prefix;
    std::ostream *stream;
    static std::ostream dummy;

   public:
    explicit Logger(Level _level, std::string _prefix)
      : level(_level), prefix(_prefix), stream(nullptr) {
      apply_level();
    }

    virtual Stream operator[](std::string subsystem) {
      *stream << prefix << ": [" << subsystem << "] ";
      return Stream(stream);
    }

    void apply_level() {
      if (level < Log::level) {
        stream = &dummy;
      } else {
        stream = Log::stream;
      }
    }
  };

  Log();
  virtual ~Log();

  static void set_file(std::ostream *stream);
  static void set_level(Log::Level level);

  static Logger Verbose;
  static Logger Debug;
  static Logger Info;
  static Logger Warn;
  static Logger Error;

 protected:
  static std::ostream *stream;
  static Level level;
};

#endif  // SRC_LOG_H_
