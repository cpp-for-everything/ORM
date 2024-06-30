#include "../lib/moka/moka.h"
#include "specs/init.hpp"

int main()
{
	Moka::Report report;
	Moka::Context("WebFrame", [](Moka::Context& it) {
		it.describe("ORM", [](Moka::Context& it) {
#include "specs/init.cpp"
		});
	}).test(report);

	return report.print();
}
