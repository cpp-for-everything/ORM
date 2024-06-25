#ifndef MOKA_TEST_H
#define MOKA_TEST_H

#include "must.h"
#include "util.h"

#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <initializer_list>

namespace Moka 
{
  template<typename... Ts>
  std::string format(std::string format_string, Ts... args) {
    std::string answer;
    answer.resize(snprintf(nullptr, 0, format_string.c_str(), args...) + 1);
    snprintf(answer.data(), answer.size(), format_string.c_str(), args...);
    return answer;
  }
}

namespace Moka
{
  class Report {
    struct Item {
      int         id;
      std::string name;
      Failure     error;
      bool        failed;
    public:
      Item(int i, std::string s, const Failure& f): id(i), name(s), error(f), failed(true) {}
      Item(int i, std::string s): id(i), name(s), failed(false) {}
    };
  protected:
    std::vector<Item> mItems;
    std::vector<std::string> mNames;
  protected:
    std::string prefix(const Item& i) const {
      if(!i.failed) {
        return cli::g("+ ", true); //✔
      }
      if(!i.error.is_fail()) {
        return cli::y("? ", true); //?
      }
      else if(i.error.file() == nullptr) {
        return cli::y("^ ", true); //▲
      }
      else {
        return cli::r("x ", true); //✘
      }
    }
  public:
    void enter(std::string name) {
      indent();
      std::cout << cli::bold(name) << "\n";
      mNames.push_back(name);
    }

    int id() const {
      return (int)mItems.size() + 1;
    }

    void indent() const {
      for(int i = (int)mNames.size(); i > 0; --i) {
        std::cout << "  ";
      }
    }

    const Item& item(int index) const {
      return mItems[index];
    }

    const std::vector<Item>& items() const {
      return mItems;
    }

    void leave() {
      mNames.pop_back();
    }

    int level() const {
      return (int)mNames.size();
    }

    int print() const {
      std::cout << "\n";
      unsigned int fails = 0;
      for(const Item& i: mItems) {
        if(!i.failed) continue;
        fails += i.error.is_fail();
        std::cout << prefix(i) << i.id << ") " << i.name << ":\n";
        std::cout << "  " << i.error.what() << "\n";

        if(i.error.file()) {
          std::cout << "  in " << cli::bold(i.error.file());
          std::cout << ':' << i.error.line() << "\n";
        }

        std::cout << "\n";
      }
      return fails;
    }

    void push(std::string testname) {
      std::stringstream stream;
      for(auto& name: mNames) stream << name << " ";
      stream << testname;

      mItems.push_back(Item(id(), stream.str()));
      summarize(mItems.back(), testname);
    }

    void push(std::string testname, const Failure& f) {
      std::stringstream stream;
      for(auto& name: mNames) stream << name << " ";
      stream << testname;

      mItems.push_back(Item(id(), stream.str(), f));
      summarize(mItems.back(), testname);
    }

    void summarize(const Item& i, const std::string& name) const {
      indent();
      std::cout << prefix(i) << i.id;
      std::cout << ") " << name << "\n";
    }
  };

  class Base {
  public:
    virtual void test(Report& report) const = 0;

    void indent(int level) {
      while(level --> 0) std::cout << "  ";
    }

    bool run() {
      Report report;
      this->test(report);
      report.print();

      return report.items().empty();
    }

    Report test() const {
      Report report;
      this->test(report);
      return report;
    }
  };

  class Test: public Base {
  protected:
    std::string           mName;
    std::function<void()> mFunction;
  public:
    Test(std::string name, std::function<void()> fn): mName(name), mFunction(fn) {
      // All done.
    }

    void test(Report& report) const {
      try {
        mFunction();
        report.push(mName);
      }
      catch(const Failure& error) {
        report.push(mName, error);
      }
      catch(const std::exception& error) {
        std::string prefix("Unexpected exception: ");
        Failure e = Failure(prefix + cli::y(error.what()));
        report.push(mName, e);
      }
    }
  };

  class Context: public Base {
  protected:
    std::string           mPrefix;
    std::string           mName;
    std::vector<const Base*>    mMembers;
    std::function<void()> mSetup;
    std::function<void()> mTeardown;
    bool                  mHasSetup;
    bool                  mHasTeardown;

    Context(std::string name, std::string prefix, std::function<void(Context&)> fn) {
      mName = std::string(name);
      mMembers = std::vector<const Base*>();
      mPrefix = std::string(prefix + " ");
      mHasTeardown = false;
      mHasSetup = false;
      fn(*this);
    }
  public:
    Context(std::string name) {
      mName = std::string(name);
      mMembers = std::vector<const Base*>();
      mPrefix = std::string("");
      mHasTeardown = false;
      mHasSetup = false;
    }

    Context(std::string name, std::function<void(Context&)> fn) {
      mName = std::string(name);
      mPrefix = std::string("");
      mMembers = std::vector<const Base*>();
      mHasTeardown = false;
      mHasSetup = false;
      fn(*this);
    }

    void describe(std::string name, std::function<void(Context&)> fn) {
      const Context* child = new Context("", name, fn);
      mMembers.push_back(child);
    }
  private:
    template<typename... Ts, size_t... ind>
    void describe_many_impl(std::string name, const std::vector<std::string>& placeholders, auto fn, std::index_sequence<ind...>) { // fn = void<T>(Context&)
      (
        (
          mMembers.push_back(new Context("", Moka::format(name, placeholders[ind].c_str()), [fn](Context& it) -> void {
            fn.template operator()<Ts>(it);
          }))
        ), ...
      );
    }
    
    template<typename... Ts, size_t... ind>
    void describe_for_impl(std::string name, const std::tuple<std::pair<Ts, std::string>...>& params, auto fn, std::index_sequence<ind...>) { // fn = void<T>(Context&)
      (
        mMembers.push_back(new Context("", Moka::format(name, std::get<ind>(params).second.c_str()), [fn, param = std::get<ind>(params).first](Context& it) -> void {
          fn.operator()(it, param);
        })), ...
      );
    }
  public:
    template<typename... Ts>
    void describe_many(std::string name, const std::initializer_list<std::string>& placeholders, auto fn) { // fn = void<T>(Context&)
      describe_many_impl<Ts...>(name, placeholders, fn, std::index_sequence_for<Ts...>{});
    }

    template<typename... Ts>
    void describe_for(std::string name, const std::tuple<std::pair<Ts, std::string>...>& params, auto fn) { // fn = void<T>(Context&)
      describe_for_impl<Ts...>(name, params, fn, std::index_sequence_for<Ts...>{});
    }

    void setup(std::function<void()> fn) {
      mHasSetup = true;
      mSetup = fn;
    }

    void should(std::string name, std::function<void()> fn) {
      const Test* test = new Test("should " + name, fn);
      mMembers.push_back(test);
    }
  private:
    template<typename Lambda, typename... Ts, size_t... ind>
    void should_many_impl(std::string name, const std::vector<std::string>& placeholders, Lambda fn, std::index_sequence<ind...>) { // fn = void<T>(Context&)
      (
        mMembers.push_back(new Test("should " + Moka::format(name, placeholders[ind].c_str()), [&fn]() -> void {
          fn.template operator()<Ts>();
        })), ...
      );
    }
    
    template<typename Lambda, typename... Ts, size_t... ind>
    void should_for_impl(std::string name, const std::tuple<std::pair<Ts, std::string>...>& params, Lambda fn, std::index_sequence<ind...>) { // fn = void<T>(Context&)
      (
        mMembers.push_back(new Test("should " + Moka::format(name, std::get<ind>(params).second.c_str()), [fn, param = std::get<ind>(params).first]() -> void {
          fn.operator()(param);
        })), ...
      );
    }

    template<typename Lambda, auto... values, size_t... ind>
    void should_for_values_impl(std::string name, const std::vector<std::string> placeholders, Lambda fn, std::index_sequence<ind...>) { // fn = void<var>()
      (
        mMembers.push_back(new Test("should " + Moka::format(name, placeholders[ind].c_str()), [fn]() -> void {
          fn.template operator()<values>();
        })), ...
      );
    }
  public:
    template<typename... Ts, typename Lambda>
    void should_many(std::string name, std::initializer_list<std::string> placeholders, Lambda fn) {
      should_many_impl<Lambda, Ts...>(name, placeholders, fn, std::index_sequence_for<Ts...>{});
    }

    template<typename... Ts, typename Lambda>
    void should_for(std::string name, std::tuple<std::pair<Ts, std::string>...> params, Lambda fn) {
      should_for_impl<Lambda, Ts...>(name, params, fn, std::index_sequence_for<Ts...>{});
    }

    template<auto... values, typename Lambda>
    void should_for_values(std::string name, std::initializer_list<std::string> placeholders, Lambda fn) {
      should_for_values_impl<Lambda, values...>(name, placeholders, fn, std::index_sequence_for<decltype(values)...>{});
    }

    void teardown(std::function<void()> fn) {
      mHasTeardown = true;
      mTeardown = fn;
    }

    void test(Report& report) const {
      report.enter(mPrefix + mName);
      if(mHasSetup) mSetup();
      for(const Base* m: mMembers) m->test(report);
      if(mHasTeardown) mTeardown();
      report.leave();
    }
  };
}

#endif
