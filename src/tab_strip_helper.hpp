#pragma once
#include <vector>
#include <functional>
#include <cstddef>
#include <utility>

struct TabContextMenuFlags {
    bool closeTab = false;
    bool closeOtherTabs = false;
    bool closeTabsToTheRight = false;
    bool reopenClosedTab = false;
    bool copyFilePath = false;
    bool openContainingFolder = false;
};

class TabStripHelper {
public:
    static TabContextMenuFlags ComputeMenuFlags(
        size_t targetIndex,
        size_t totalTabs,
        bool hasFile,
        bool hasClosedHistory
    ) {
        TabContextMenuFlags flags;
        flags.closeTab = (totalTabs > 0 && targetIndex < totalTabs);
        flags.closeOtherTabs = (totalTabs > 1 && targetIndex < totalTabs);
        flags.closeTabsToTheRight = (totalTabs > 0 && targetIndex + 1 < totalTabs);
        flags.reopenClosedTab = hasClosedHistory;
        flags.copyFilePath = hasFile;
        flags.openContainingFolder = hasFile;
        return flags;
    }

    template <typename T, typename Predicate, typename Callback>
    static bool CloseOtherTabs(
        std::vector<T>& tabs,
        size_t& activeIndex,
        size_t keepIndex,
        Predicate canClosePredicate,
        Callback onClosedCallback
    ) {
        if (keepIndex >= tabs.size() || tabs.size() <= 1) {
            return false;
        }

        // 1. Validate all tabs to close
        for (size_t i = 0; i < tabs.size(); ++i) {
            if (i == keepIndex) continue;
            if (!canClosePredicate(i, tabs[i])) {
                return false; // User cancelled
            }
        }

        // 2. Trigger onClosedCallback for history tracking
        for (size_t i = 0; i < tabs.size(); ++i) {
            if (i == keepIndex) continue;
            onClosedCallback(i, tabs[i]);
        }

        // 3. Keep target tab and clear all others
        T kept = std::move(tabs[keepIndex]);
        tabs.clear();
        tabs.push_back(std::move(kept));
        activeIndex = 0;
        return true;
    }

    template <typename T, typename Predicate, typename Callback>
    static bool CloseTabsToTheRight(
        std::vector<T>& tabs,
        size_t& activeIndex,
        size_t index,
        Predicate canClosePredicate,
        Callback onClosedCallback
    ) {
        if (index + 1 >= tabs.size()) {
            return false;
        }

        // 1. Validate all tabs to the right
        for (size_t i = index + 1; i < tabs.size(); ++i) {
            if (!canClosePredicate(i, tabs[i])) {
                return false; // User cancelled
            }
        }

        // 2. Trigger onClosedCallback
        for (size_t i = index + 1; i < tabs.size(); ++i) {
            onClosedCallback(i, tabs[i]);
        }

        // 3. Erase tabs to the right
        tabs.erase(tabs.begin() + (index + 1), tabs.end());

        // 4. Ensure activeIndex <= index
        if (activeIndex > index) {
            activeIndex = index;
        }
        return true;
    }
};
