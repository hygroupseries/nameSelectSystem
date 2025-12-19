#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class RosterManager
{
public:
    struct Student
    {
        std::string name;
        std::string group;
        int callCount = 0;
    };

    struct CallRecord
    {
        std::string name;
        std::string group;
        std::chrono::system_clock::time_point timestamp;
    };

    struct ImportStats
    {
        std::size_t added = 0;
        std::size_t duplicates = 0;
        std::size_t malformed = 0;
    };

    RosterManager() : rng_(std::random_device{}()) {}

    bool addStudent(const std::string &name, const std::string &group, bool refreshPools = true)
    {
        auto duplicate = std::find_if(
            students_.begin(), students_.end(),
            [&](const Student &s)
            { return s.name == name; });
        if (duplicate != students_.end())
        {
            return false;
        }

        students_.push_back(Student{name, group, 0});
        if (refreshPools)
        {
            resetCycle();
        }
        return true;
    }

    std::optional<ImportStats> importFromFile(const std::string &path)
    {
        std::ifstream input(path);
        if (!input.is_open())
        {
            return std::nullopt;
        }

        ImportStats stats;
        loadFromStream(input, stats);
        resetCycle();
        return stats;
    }

    std::optional<Student> pickRandom(const std::optional<std::string> &group = std::nullopt)
    {
        if (students_.empty())
        {
            return std::nullopt;
        }

        if (group)
        {
            if (groupPools_[*group].empty())
            {
                refillGroupPool(*group);
            }
            if (groupPools_[*group].empty())
            {
                return std::nullopt;
            }
            return consumeIndex(groupPools_[*group]);
        }

        if (globalPool_.empty())
        {
            refillGlobalPool();
        }
        if (globalPool_.empty())
        {
            return std::nullopt;
        }
        return consumeIndex(globalPool_);
    }

    void printHistory(std::size_t limit = 0) const
    {
        if (history_.empty())
        {
            std::cout << "No history yet\n";
            return;
        }

        std::size_t count = 0;
        for (auto it = history_.rbegin(); it != history_.rend(); ++it)
        {
            const auto time = std::chrono::system_clock::to_time_t(it->timestamp);
            std::cout << std::put_time(std::localtime(&time), "%F %T")
                      << " - " << it->group << " - " << it->name << '\n';
            ++count;
            if (limit && count >= limit)
            {
                break;
            }
        }
    }

    void printStats() const
    {
        if (students_.empty())
        {
            std::cout << "No student data\n";
            return;
        }

        std::vector<const Student *> ordered;
        ordered.reserve(students_.size());
        for (const auto &s : students_)
        {
            ordered.push_back(&s);
        }
        std::sort(ordered.begin(), ordered.end(), [](const Student *lhs, const Student *rhs)
                  {
            if (lhs->callCount == rhs->callCount) {
                return lhs->name < rhs->name;
            }
            return lhs->callCount > rhs->callCount; });

        std::cout << std::left << std::setw(20) << "Name" << std::setw(15) << "Group" << "Count" << '\n';
        for (const auto *s : ordered)
        {
            std::cout << std::left << std::setw(20) << s->name << std::setw(15) << s->group
                      << s->callCount << '\n';
        }
    }

    void listGroups() const
    {
        if (students_.empty())
        {
            std::cout << "No group data\n";
            return;
        }

        std::unordered_map<std::string, int> counts;
        for (const auto &s : students_)
        {
            ++counts[s.group];
        }
        std::cout << "Groups:\n";
        for (const auto &entry : counts)
        {
            std::cout << "- " << entry.first << " (" << entry.second << ")\n";
        }
    }

    void resetCycle()
    {
        globalPool_.clear();
        groupPools_.clear();
    }

    void clearHistory()
    {
        history_.clear();
    }

private:
    void loadFromStream(std::istream &input, ImportStats &stats)
    {
        std::string line;
        while (std::getline(input, line))
        {
            const auto trimmed = trim(line);
            if (trimmed.empty() || trimmed.front() == '#')
            {
                continue;
            }

            const auto comma = trimmed.find(',');
            if (comma == std::string::npos)
            {
                ++stats.malformed;
                continue;
            }

            auto name = trim(trimmed.substr(0, comma));
            auto group = trim(trimmed.substr(comma + 1));
            if (name.empty() || group.empty())
            {
                ++stats.malformed;
                continue;
            }

            if (addStudent(name, group, false))
            {
                ++stats.added;
            }
            else
            {
                ++stats.duplicates;
            }
        }
    }

    static std::string trim(const std::string &text)
    {
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return {};
        }
        const auto last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    Student consumeIndex(std::deque<std::size_t> &pool)
    {
        const auto idx = pool.front();
        pool.pop_front();
        auto &student = students_[idx];
        ++student.callCount;
        history_.push_back(CallRecord{student.name, student.group, std::chrono::system_clock::now()});
        return student;
    }

    void refillGlobalPool()
    {
        std::vector<std::size_t> indices(students_.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng_);
        globalPool_.assign(indices.begin(), indices.end());
    }

    void refillGroupPool(const std::string &group)
    {
        std::vector<std::size_t> indices;
        for (std::size_t i = 0; i < students_.size(); ++i)
        {
            if (students_[i].group == group)
            {
                indices.push_back(i);
            }
        }
        if (indices.empty())
        {
            groupPools_.erase(group);
            return;
        }
        std::shuffle(indices.begin(), indices.end(), rng_);
        groupPools_[group].assign(indices.begin(), indices.end());
    }

    std::vector<Student> students_;
    std::unordered_map<std::string, std::deque<std::size_t>> groupPools_;
    std::deque<std::size_t> globalPool_;
    std::list<CallRecord> history_;
    std::mt19937 rng_;
};

void printMenu()
{
    std::cout << "\n=== Random Roll Call System ===\n"
              << "1. Add student\n"
              << "2. Call random student\n"
              << "3. Call by group\n"
              << "4. Show history\n"
              << "5. Show statistics\n"
              << "6. Show groups\n"
              << "7. Reset cycle\n"
              << "8. Clear history\n"
              << "9. Import from CSV\n"
              << "0. Exit\n"
              << "Select: ";
}

int main()
{
    RosterManager manager;
    const std::string defaultRoster = "roster.csv";
    if (auto stats = manager.importFromFile(defaultRoster))
    {
        std::cout << "Loaded default roster from " << defaultRoster << ". Added "
                  << stats->added << ", duplicates " << stats->duplicates
                  << ", malformed " << stats->malformed << ".\n";
    }
    else
    {
        std::cout << "No default roster found. Use option 9 to import manually.\n";
    }
    bool running = true;

    while (running)
    {
        printMenu();
        int option = -1;
        if (!(std::cin >> option))
        {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input, try again.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (option)
        {
        case 1:
        {
            std::string name;
            std::string group;
            std::cout << "Student name: ";
            std::getline(std::cin, name);
            std::cout << "Group name: ";
            std::getline(std::cin, group);
            if (name.empty() || group.empty())
            {
                std::cout << "Name and group cannot be empty.\n";
                break;
            }
            if (manager.addStudent(name, group))
            {
                std::cout << "Student added.\n";
            }
            else
            {
                std::cout << "Student already exists.\n";
            }
            break;
        }
        case 2:
        {
            auto student = manager.pickRandom();
            if (student)
            {
                std::cout << "Selected: " << student->name << " (" << student->group << ")\n";
            }
            else
            {
                std::cout << "No students available.\n";
            }
            break;
        }
        case 3:
        {
            std::string group;
            std::cout << "Group to call: ";
            std::getline(std::cin, group);
            if (group.empty())
            {
                std::cout << "Group name cannot be empty.\n";
                break;
            }
            auto student = manager.pickRandom(std::optional<std::string>{group});
            if (student)
            {
                std::cout << "Selected: " << student->name << " (" << student->group << ")\n";
            }
            else
            {
                std::cout << "Group empty or all called.\n";
            }
            break;
        }
        case 4:
        {
            std::size_t limit = 0;
            std::cout << "How many recent records to show (0 = all): ";
            std::cin >> limit;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            manager.printHistory(limit);
            break;
        }
        case 5:
            manager.printStats();
            break;
        case 6:
            manager.listGroups();
            break;
        case 7:
            manager.resetCycle();
            std::cout << "Cycle reset.\n";
            break;
        case 8:
            manager.clearHistory();
            std::cout << "History cleared.\n";
            break;
        case 9:
        {
            std::string path;
            std::cout << "CSV file path (name,group per line): ";
            std::getline(std::cin, path);
            if (path.empty())
            {
                std::cout << "Path cannot be empty.\n";
                break;
            }

            const auto result = manager.importFromFile(path);
            if (!result)
            {
                std::cout << "Failed to open file.\n";
                break;
            }

            std::cout << "Imported " << result->added << " new students, "
                      << result->duplicates << " duplicates, "
                      << result->malformed << " malformed lines.\n";
            break;
        }
        case 0:
            running = false;
            break;
        default:
            std::cout << "Unknown option.\n";
            break;
        }
    }

    std::cout << "Goodbye!\n";
    return 0;
}
