#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include "../src/tab_strip_helper.hpp"

struct DummyTab {
    int id = 0;
    std::string name;
    bool isDirty = false;
};

void TestMenuFlagsSingleTab() {
    std::cout << "[Test] Menu Flags - Single Tab: ";

    // Single tab, no file, no history
    auto f1 = TabStripHelper::ComputeMenuFlags(0, 1, false, false);
    assert(f1.closeTab == true);
    assert(f1.closeOtherTabs == false);
    assert(f1.closeTabsToTheRight == false);
    assert(f1.reopenClosedTab == false);
    assert(f1.copyFilePath == false);
    assert(f1.openContainingFolder == false);

    // Single tab, with file and history
    auto f2 = TabStripHelper::ComputeMenuFlags(0, 1, true, true);
    assert(f2.closeTab == true);
    assert(f2.closeOtherTabs == false);
    assert(f2.closeTabsToTheRight == false);
    assert(f2.reopenClosedTab == true);
    assert(f2.copyFilePath == true);
    assert(f2.openContainingFolder == true);

    std::cout << "PASSED" << std::endl;
}

void TestMenuFlagsMultipleTabs() {
    std::cout << "[Test] Menu Flags - Multiple Tabs: ";

    // 3 tabs: index 0 (left-most)
    auto f0 = TabStripHelper::ComputeMenuFlags(0, 3, true, false);
    assert(f0.closeTab == true);
    assert(f0.closeOtherTabs == true);
    assert(f0.closeTabsToTheRight == true);
    assert(f0.copyFilePath == true);

    // 3 tabs: index 1 (middle)
    auto f1 = TabStripHelper::ComputeMenuFlags(1, 3, false, true);
    assert(f1.closeTab == true);
    assert(f1.closeOtherTabs == true);
    assert(f1.closeTabsToTheRight == true);
    assert(f1.reopenClosedTab == true);
    assert(f1.copyFilePath == false);

    // 3 tabs: index 2 (right-most)
    auto f2 = TabStripHelper::ComputeMenuFlags(2, 3, true, true);
    assert(f2.closeTab == true);
    assert(f2.closeOtherTabs == true);
    assert(f2.closeTabsToTheRight == false); // Right-most tab: must be disabled
    assert(f2.reopenClosedTab == true);
    assert(f2.copyFilePath == true);

    // Out of bounds target index
    auto fOut = TabStripHelper::ComputeMenuFlags(5, 3, true, true);
    assert(fOut.closeTab == false);
    assert(fOut.closeOtherTabs == false);
    assert(fOut.closeTabsToTheRight == false);

    std::cout << "PASSED" << std::endl;
}

void TestCloseTabsToTheRight() {
    std::cout << "[Test] CloseTabsToTheRight: ";

    // Case 1: 4 tabs, target index 1, active tab was index 3 (to be closed)
    {
        std::vector<DummyTab> tabs = {
            { 1, "Tab1", false },
            { 2, "Tab2", false },
            { 3, "Tab3", false },
            { 4, "Tab4", false }
        };
        size_t activeIdx = 3;
        std::vector<int> closedIds;

        bool res = TabStripHelper::CloseTabsToTheRight<DummyTab>(
            tabs,
            activeIdx,
            1,
            [](size_t, DummyTab&) { return true; },
            [&closedIds](size_t, DummyTab& tab) { closedIds.push_back(tab.id); }
        );

        assert(res == true);
        assert(tabs.size() == 2);
        assert(tabs[0].id == 1 && tabs[0].name == "Tab1");
        assert(tabs[1].id == 2 && tabs[1].name == "Tab2");
        assert(activeIdx == 1); // adjusted from 3 to 1
        assert(closedIds.size() == 2);
        assert(closedIds[0] == 3 && closedIds[1] == 4);
    }

    // Case 2: Target is right-most tab (cannot close right)
    {
        std::vector<DummyTab> tabs = {
            { 1, "Tab1", false },
            { 2, "Tab2", false }
        };
        size_t activeIdx = 0;
        bool res = TabStripHelper::CloseTabsToTheRight(
            tabs,
            activeIdx,
            1,
            [](size_t, DummyTab&) { return true; },
            [](size_t, DummyTab&) {}
        );
        assert(res == false);
        assert(tabs.size() == 2);
        assert(activeIdx == 0);
    }

    // Case 3: Target index 0, active tab was index 0
    {
        std::vector<DummyTab> tabs = {
            { 1, "Tab1", false },
            { 2, "Tab2", false },
            { 3, "Tab3", false }
        };
        size_t activeIdx = 0;
        bool res = TabStripHelper::CloseTabsToTheRight(
            tabs,
            activeIdx,
            0,
            [](size_t, DummyTab&) { return true; },
            [](size_t, DummyTab&) {}
        );
        assert(res == true);
        assert(tabs.size() == 1);
        assert(tabs[0].id == 1);
        assert(activeIdx == 0); // remains 0
    }

    // Case 4: Predicate rejection (e.g. user cancels save prompt)
    {
        std::vector<DummyTab> tabs = {
            { 1, "Tab1", false },
            { 2, "Tab2", false },
            { 3, "Tab3", true }
        };
        size_t activeIdx = 0;
        bool res = TabStripHelper::CloseTabsToTheRight(
            tabs,
            activeIdx,
            0,
            [](size_t, DummyTab& tab) { return !tab.isDirty; }, // Tab 3 rejects
            [](size_t, DummyTab&) {}
        );
        assert(res == false);
        assert(tabs.size() == 3); // nothing was erased
        assert(activeIdx == 0);
    }

    std::cout << "PASSED" << std::endl;
}

void TestCloseOtherTabs() {
    std::cout << "[Test] CloseOtherTabs: ";

    // Case 1: 3 tabs, keep index 1
    {
        std::vector<DummyTab> tabs = {
            { 10, "First", false },
            { 20, "Target", false },
            { 30, "Last", false }
        };
        size_t activeIdx = 2;
        std::vector<int> closedIds;

        bool res = TabStripHelper::CloseOtherTabs<DummyTab>(
            tabs,
            activeIdx,
            1,
            [](size_t, DummyTab&) { return true; },
            [&closedIds](size_t, DummyTab& tab) { closedIds.push_back(tab.id); }
        );

        assert(res == true);
        assert(tabs.size() == 1);
        assert(tabs[0].id == 20 && tabs[0].name == "Target");
        assert(activeIdx == 0);
        assert(closedIds.size() == 2);
        assert(closedIds[0] == 10 && closedIds[1] == 30);
    }

    // Case 2: Only 1 tab present (cannot close others)
    {
        std::vector<DummyTab> tabs = {
            { 1, "OnlyTab", false }
        };
        size_t activeIdx = 0;
        bool res = TabStripHelper::CloseOtherTabs(
            tabs,
            activeIdx,
            0,
            [](size_t, DummyTab&) { return true; },
            [](size_t, DummyTab&) {}
        );
        assert(res == false);
        assert(tabs.size() == 1);
    }

    // Case 3: Predicate cancels operation
    {
        std::vector<DummyTab> tabs = {
            { 1, "Tab1", true },
            { 2, "Tab2", false }
        };
        size_t activeIdx = 1;
        bool res = TabStripHelper::CloseOtherTabs(
            tabs,
            activeIdx,
            1,
            [](size_t, DummyTab& tab) { return !tab.isDirty; }, // Tab 1 cancels
            [](size_t, DummyTab&) {}
        );
        assert(res == false);
        assert(tabs.size() == 2); // preserved
        assert(activeIdx == 1);
    }

    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  LightNotePad Tab Strip Unit Tests                       " << std::endl;
    std::cout << "==========================================================" << std::endl;

    TestMenuFlagsSingleTab();
    TestMenuFlagsMultipleTabs();
    TestCloseTabsToTheRight();
    TestCloseOtherTabs();

    std::cout << "All Tab Strip unit tests completed successfully!" << std::endl;
    return 0;
}
