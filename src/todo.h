#pragma once

#include <string>
#include <vector>
#include <optional>

struct Todo {
    int id = 0;
    std::string text;
    bool completed = false;
    std::string created_date;
    std::string completed_date;
    std::vector<std::string> notes;
};

enum class TodoFilter { Open, Closed, All };

// Return the next stable ID to assign (max existing ID + 1, or 1 if empty).
int next_todo_id(const std::vector<Todo>& todos);

// Find a todo by ID. Returns nullptr if not found.
Todo* find_todo(std::vector<Todo>& todos, int id);
const Todo* find_todo(const std::vector<Todo>& todos, int id);

// Filter todos by status.
std::vector<const Todo*> filter_todos(const std::vector<Todo>& todos, TodoFilter filter);

struct TodoResult {
    bool ok = true;
    bool warned = false;   // true when a non-fatal warning applies
    std::string message;   // warning or error text
};

// Mark a todo completed. Returns warned=true if already completed.
TodoResult complete_todo(std::vector<Todo>& todos, int id, const std::string& date);

// Add a note to a todo. Returns warned=true if todo is already completed.
TodoResult add_note(std::vector<Todo>& todos, int id, const std::string& note);
