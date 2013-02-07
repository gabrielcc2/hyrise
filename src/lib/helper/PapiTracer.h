// Copyright (c) 2012 Hasso-Plattner-Institut fuer Softwaresystemtechnik GmbH. All rights reserved.
#ifndef SRC_LIB_HELPER_PAPITRACER_H_
#define SRC_LIB_HELPER_PAPITRACER_H_

#include <stdio.h>
#include <sys/time.h>

#include <algorithm>
#include <vector>
#include <string>
#include <stdexcept>

#include "helper/stringhelpers.h"

/// For errors generally related to tracing
class TracingError : public std::runtime_error {
 public:
  TracingError(std::string what) : std::runtime_error("TracingError: " + what) {}
};

#ifdef USE_PAPI_TRACE
#include <mutex>

#include "papi.h"
/// Tracing wrapper for PAPI
///
/// Usage:
///
///     PapiTracer pt;
///     pt.addEvent("PAPI_TOT_INS"); // add counter for total instructions
///     pt.start();
///     /* do some work */
///     pt.stop();
///     std::cout << pt.value("PAPI_TOT_INS") << std::endl;
///
/// Multiple events can be added but may or may not work due to restrictions
/// of the underlying hardware.
class PapiTracer {
 public:
  typedef long long result_t;
 private:
  //! the initialized eventset from PAPI
  int _eventSet;
  bool _disabled;
  bool _running;
  //! keeps track of registered counters
  std::vector<std::string> _counters;
  //! performance counter results
  std::vector<result_t> _results;

  /// Shorthand for error testing with PAPI functions
  ///
  /// @param[in] function PAPI function to call that returns PAPI error codes
  /// @param[in] activity Information about the activity currently conducted, for error reporting
  /// @param[in] args parameters of the function to call
  template<typename Func, typename... Args>
  static void handle(Func function, const char* activity, Args&&... args) {
    handle(function, std::string(activity), args...);
  }

  template<typename Func, typename... Args>
  static void handle(Func function, const std::string& activity, Args&&... args) {
    int retval = function(std::forward<Args>(args)...);
    if (retval != PAPI_OK)
      throw TracingError(activity + " failed: " + PAPI_strerror(retval));
  }

  static void initialize() {
    static bool initialized = false;
    static std::mutex init_mtx;

    std::lock_guard<std::mutex> guard(init_mtx);
    if (!initialized) {
      if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT)
        throw TracingError("PAPI could not be initialized");
      initialized = true;
    }
  }

 public:
  inline PapiTracer() : _eventSet(PAPI_NULL), _disabled(false), _running(false) {
    PapiTracer::initialize();

    if (PAPI_thread_init(pthread_self) != PAPI_OK)
      throw TracingError("PAPI could not initialize thread");

    int retval;
    retval = PAPI_create_eventset(&_eventSet);
    if (retval != PAPI_OK) {
      printf("PAPI eventset creation failed");
      exit(-1);
    }
  }

  inline ~PapiTracer() {
    if (_running) stop();
    handle(PAPI_cleanup_eventset, "cleaning eventset", _eventSet);
    handle(PAPI_destroy_eventset, "destroying eventset", &_eventSet);
  }

  /// Add a new event counter
  ///
  /// @param[in] eventName valid PAPI event name (see `$ papi_avail`)
  inline void addEvent(std::string eventName) {
    if (eventName == "NO_PAPI") {
      _disabled = true;
      return;
    }
    _counters.push_back(eventName);
  }

  /// Start performance counter
  inline void start() {
    if (_disabled)
      return;

    if (_counters.empty())
      throw std::runtime_error("No events set");

    for(const auto& eventName: _counters) {
      int eventCode;
      handle(PAPI_event_name_to_code,
             "Create event from " + eventName,
             (char*) eventName.c_str(), &eventCode);
      handle(PAPI_add_event, "Adding event to event set", _eventSet, eventCode);
    }

    _results.clear();
    _results.resize(_counters.size());

    _running = true;
    handle(PAPI_reset, "Reset counter", _eventSet);
    handle(PAPI_start, "Starting counter", _eventSet);
  }

  /// Stop performance counter
  inline void stop() {
    if (_disabled) return;

    handle(PAPI_stop, "Stopping Counter", _eventSet, _results.data());
    _running = false;
  }

  /// Resets performance counters
  inline void reset() {
    if (_disabled) return;

    stop();
    _results.clear();
    handle(PAPI_reset, "Reset counter", _eventSet);
  }

  /// Retrieve performance counter values
  inline long long value(const std::string& eventName) const {
    if (_disabled) return 0ull;

    auto item = std::find(_counters.begin(), _counters.end(), eventName);
    if (item == _counters.end())
      throw TracingError("Trying to access unregistered event '" + eventName +"' "
                         "Available: " + joinString(_counters, " "));
    auto index = std::distance(_counters.begin(), item);
    return _results.at(index);
  }
};

#else

/// Fallback tracing mechanism that behaves like PapiTracer
/// concerning adding of new events to measure but will
/// only return the time elapsed time in microseconds. This is useful
/// for systems without PAPI support such as virtual machines
///
/// @note This tracer does not check for PAPI event name validity!
class FallbackTracer {
 public:
  typedef long long result_t;
 private:
  std::vector<std::string> _counters;
  result_t _result;
  struct timeval _start;
 public:
  inline void addEvent(std::string eventName) {
    _counters.push_back(eventName);
  }

  inline void start() {
    if (_counters.empty())
      throw TracingError("No events set");
    _result = 0;
    gettimeofday(&_start, nullptr);
  }

  inline void stop() {
    struct timeval end = {0, 0};
    gettimeofday(&end, nullptr);
    _result = (end.tv_sec - _start.tv_sec) * 1000000 + (end.tv_usec - _start.tv_usec);
  }  

  inline void reset() {
    _start = {0, 0};
    _result = 0;
  }
  
  inline long long value(const std::string& eventName) const {
    auto item = find(_counters.begin(), _counters.end(), eventName);
    if (item == _counters.end())
      throw TracingError("Trying to access unregistered event '" + eventName +"' "
                         "Available: " + joinString(_counters, " "));
    return _result;
  }
};

typedef FallbackTracer PapiTracer;

#endif


#endif  // SRC_LIB_HELPER_PAPITRACER_H_
