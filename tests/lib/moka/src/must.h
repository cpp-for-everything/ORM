#ifndef MOKA_MUST_H
#define MOKA_MUST_H

#include "util.h"

#include <cstring>
#include <exception>
#include <functional>
#include <sstream>
#include <string>

#define must_contain(a, b, msg)       Moka::must::contain(__FILE__, __LINE__, a, b, msg)
#define must_be_equal(a, b, msg)      Moka::must::be_equal(__FILE__, __LINE__, a, b, msg)
#define must_be_less(a, b, msg)       Moka::must::be_less(__FILE__, __LINE__, a, b, msg)
#define must_be_greater(a, b, msg)    Moka::must::be_greater(__FILE__, __LINE__, a, b, msg)
#define must_be_not_equal(a, b, msg)  Moka::must::be_not_equal(__FILE__, __LINE__, a, b, msg)
#define must_throw(T, f, msg)         Moka::must::throoow<T>(__FILE__, __LINE__, #T, f, msg)
#define must_fail(m, msg)             Moka::must::fail(__FILE__, __LINE__, m, msg)

#define would_be_nice_to_contain(a, b, msg)       Moka::would_be_nice_to::contain(__FILE__, __LINE__, a, b, msg)
#define would_be_nice_to_be_equal(a, b, msg)      Moka::would_be_nice_to::be_equal(__FILE__, __LINE__, a, b, msg)
#define would_be_nice_to_be_less(a, b, msg)       Moka::would_be_nice_to::be_less(__FILE__, __LINE__, a, b, msg)
#define would_be_nice_to_be_greater(a, b, msg)    Moka::would_be_nice_to::be_greater(__FILE__, __LINE__, a, b, msg)
#define would_be_nice_to_be_not_equal(a, b, msg)  Moka::would_be_nice_to::be_not_equal(__FILE__, __LINE__, a, b, msg)
#define would_be_nice_to_throw(T, f, msg)         Moka::would_be_nice_to::throoow<T>(__FILE__, __LINE__, #T, f, msg)
#define would_be_nice_to_fail(m, msg)             Moka::would_be_nice_to::fail(__FILE__, __LINE__, m, msg)

namespace Moka
{
  class Failure: public std::exception {
    const char* mFile;
    const int   mLine;
    std::string mWhat;
    bool        mFail;
  public:
    Failure(): mFile(nullptr), mLine(0), mWhat(""), mFail(false) {}
    Failure(const std::string& m, bool is_fail = true): mFile(nullptr), mLine(0), mWhat(m), mFail(is_fail) {}
    Failure(const char* f, int l, const std::string& m, bool is_fail = true): mFile(f), mLine(l), mWhat(m), mFail(is_fail) {}
    ~Failure() throw() {}

    const char* file() const {
      return mFile;
    }

    int line() const {
      return mLine;
    }

    const char* what() const throw() {
      return mWhat.c_str();
    }

    bool is_fail() const {
      return mFail;
    }
  };

  namespace must {
    void contain(const char* f, int l, const std::string& a, const std::string& b, const std::string& msg = "") {
      if(b.find(a) == std::string::npos) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected " << cli::g(b) << " to contain " << cli::r(a);
        throw Failure(f, l, message.str());
      }
    }
    
    template <class A, class B>
    void be_equal(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(a != b) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str());
      }
    }   

    template <class A, class B>
    void be_less(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(!(a < b)) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str());
      }
    }

    template <class A, class B>
    void be_greater(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(!(a > b)) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str());
      }
    }

    void be_equal(const char* f, int l, const char* a, const char* b, const std::string& msg = "") {
      if(strcmp(a, b) != 0) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str());
      }
    }

    void fail(const char* f, int l,  const char* m) {
      std::stringstream message;
      message << cli::r(m);
      throw Failure(f, l, message.str());
    }

    template <class A, class B>
    void be_not_equal(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(a == b) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected anything but " << cli::r(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str());
      }
    }

    template <class E>
    void throoow(const char* f, int l, const char* type, std::function<void()> fn, const std::string& msg = "") {
      try {
        fn();
      }
      catch(E& e) {
        // Good catch!
        return;
      }
      catch(std::exception& e) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Expected to catch a " << cli::g(type) << " but got " << cli::r(e.what());
        throw Failure(f, l, message.str());
      }

      std::stringstream message;
      message << ((msg.size() > 0) ? msg + " | " : "") << "Expected to catch a " << cli::g(type) << " but nothing was thrown!";
      throw Failure(f, l, message.str());
    }
  }
  namespace would_be_nice_to {
    void contain(const char* f, int l, const std::string& a, const std::string& b, const std::string& msg = "") {
      if(b.find(a) == std::string::npos) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice " << cli::g(b) << " to contain " << cli::r(a);
        throw Failure(f, l, message.str(), false);
      }
    }
    
    template <class A, class B>
    void be_equal(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(a != b) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str(), false);
      }
    }

    template <class A, class B>
    void be_less(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(!(a < b)) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str(), false);
      }
    }

    template <class A, class B>
    void be_greater(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(!(a > b)) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str(), false);
      }
    }

    void be_equal(const char* f, int l, const char* a, const char* b, const std::string& msg = "") {
      if(strcmp(a, b) != 0) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice " << cli::g(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str(), false);
      }
    }

    void fail(const char* f, int l,  const char* m) {
      std::stringstream message;
      message << cli::r(m);
      throw Failure(f, l, message.str(), false);
    }

    template <class A, class B>
    void be_not_equal(const char* f, int l, const A& a, const B& b, const std::string& msg = "") {
      if(a == b) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice anything but " << cli::r(b) << " but got " << cli::r(a);
        throw Failure(f, l, message.str(), false);
      }
    }

    template <class E>
    void throoow(const char* f, int l, const char* type, std::function<void()> fn, const std::string& msg = "") {
      try {
        fn();
      }
      catch(E& e) {
        // Good catch!
        return;
      }
      catch(std::exception& e) {
        std::stringstream message;
        message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice to catch a " << cli::g(type) << " but got " << cli::r(e.what());
        throw Failure(f, l, message.str(), false);
      }

      std::stringstream message;
      message << ((msg.size() > 0) ? msg + " | " : "") << "Would have been nice to catch a " << cli::g(type) << " but nothing was thrown!";
      throw Failure(f, l, message.str(), false);
    }
  }
}

#endif
