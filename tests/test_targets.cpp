#include <set>
#include <string>

#include "basic/target.hpp"
#include "test_util.hpp"

using namespace basic;

int main() {
    // every target has a unique name and can be found by it
    std::set<std::string> names;
    for (const TargetInfo& t : targets()) {
        CHECK(names.insert(std::string(t.name)).second);
        const auto found = findTarget(t.name);
        CHECK(found.has_value());
        if (found) CHECK_EQ(found->name, t.name);
        CHECK(contains(t.supported, Group::Core));  // the language core is always there
    }
    CHECK_EQ(names.size(), std::size_t{5});
    CHECK(!findTarget("c64").has_value());

    // the CPU backends of the planned targets
    CHECK(findTarget("amiga")->cpu == Cpu::M68000);
    CHECK(findTarget("cpc")->cpu == Cpu::Z80);
    CHECK(findTarget("pc")->cpu == Cpu::Host);

    // platform scope as documented in docs/language.md
    CHECK(contains(findTarget("amiga")->supported, Group::Gui));
    CHECK(contains(findTarget("amiga")->supported, Group::AmigaOs));
    CHECK(!contains(findTarget("cpc")->supported, Group::Gui));
    CHECK(!contains(findTarget("cpc")->supported, Group::FileRandom));  // AMSDOS: one file in, one out
    CHECK(contains(findTarget("cpc")->supported, Group::Graphics));
    CHECK(contains(findTarget("z80-test")->supported, Group::FileSeq));  // the RAM disk of the test driver
    CHECK(!contains(findTarget("z80-test")->supported, Group::Graphics));

    // group names are unique and stable
    std::set<std::string> groups;
    for (Group g : allGroups()) CHECK(groups.insert(std::string(groupName(g))).second);
    CHECK_EQ(groupName(Group::FileRandom), std::string_view("file-random"));

    const std::string table = formatTargets();
    CHECK(table.find("cpc  (z80, platform cpc)") != std::string::npos);
    return report("targets");
}
