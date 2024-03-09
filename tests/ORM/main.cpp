#include "../lib/moka/moka.h"
#include "specs/init.hpp"

int main() {
  Moka::Report report;
  Moka::Context("WebFrame", [](Moka::Context &it) {
    it.describe("ORM", [](Moka::Context &it) {
      FieldsTests::init(it);
      TableTests::init(it);
      RulesTests::init(it);
    });
  }).test(report);

  return report.print();
}