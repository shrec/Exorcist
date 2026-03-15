/**
 * @file sandbox_test.cpp
 * @brief სატესტო ფაილი — agent-მა დაწერა Exorcist IDE-ში
 *
 * ეს ფაილი ამოწმებს რომ agent-ს შეუძლია:
 *   1. ფაილის შექმნა
 *   2. კოდის დაწერა
 *   3. ედიტორში გახსნა
 */

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

struct Task {
    int id;
    std::string title;
    bool completed;
};

std::vector<Task> createSampleTasks()
{
    return {
        {1, "Project Brain init",     true},
        {2, "Rules.json setup",       true},
        {3, "Agent Memory DB",        false},
        {4, "Lua commit-prep tool",   false},
        {5, "Debug log cleanup",      false},
    };
}

void printTasks(const std::vector<Task>& tasks)
{
    std::cout << "=== Exorcist TODO ===" << std::endl;
    for (const auto& t : tasks) {
        std::cout << (t.completed ? "[x] " : "[ ] ")
                  << t.id << ". " << t.title << std::endl;
    }

    auto done = std::count_if(tasks.begin(), tasks.end(),
                              [](const Task& t) { return t.completed; });

    std::cout << "\nProgress: " << done << "/" << tasks.size()
              << " (" << (done * 100 / tasks.size()) << "%)" << std::endl;
}

int main()
{
    auto tasks = createSampleTasks();
    printTasks(tasks);
    return 0;
}
