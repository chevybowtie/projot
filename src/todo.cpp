#include "todo.h"

int next_todo_id(const std::vector<Todo>& todos) {
    if (todos.empty()) return 1;
    int max_id = 0;
    for (const auto& t : todos) {
        if (t.id > max_id) max_id = t.id;
    }
    return max_id + 1;
}

Todo* find_todo(std::vector<Todo>& todos, int id) {
    for (auto& t : todos) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const Todo* find_todo(const std::vector<Todo>& todos, int id) {
    for (const auto& t : todos) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

std::vector<const Todo*> filter_todos(const std::vector<Todo>& todos, TodoFilter filter) {
    std::vector<const Todo*> result;
    for (const auto& t : todos) {
        if (filter == TodoFilter::All ||
            (filter == TodoFilter::Open  && t.status != TodoStatus::Done) ||
            (filter == TodoFilter::Closed && t.status == TodoStatus::Done)) {
            result.push_back(&t);
        }
    }
    return result;
}

TodoResult complete_todo(std::vector<Todo>& todos, int id, const std::string& date) {
    auto* t = find_todo(todos, id);
    if (!t) return {false, false, "Todo ID " + std::to_string(id) + " not found."};

    if (t->status == TodoStatus::Done) {
        return {true, true, "Todo " + std::to_string(id) + " is already completed."};
    }

    t->status = TodoStatus::Done;
    t->completed_date = date;
    return {true, false, ""};
}

TodoResult set_todo_status(std::vector<Todo>& todos, int id, TodoStatus status, const std::string& date) {
    auto* t = find_todo(todos, id);
    if (!t) return {false, false, "todo " + std::to_string(id) + " not found."};

    if (t->status == status) {
        return {true, true, "todo " + std::to_string(id) + " is already at that status."};
    }

    t->status = status;
    if (status == TodoStatus::Done) {
        t->completed_date = date;
    } else {
        t->completed_date.clear();
    }
    return {true, false, ""};
}

TodoResult add_note(std::vector<Todo>& todos, int id, const std::string& note) {
    auto* t = find_todo(todos, id);
    if (!t) return {false, false, "Todo ID " + std::to_string(id) + " not found."};

    TodoResult result{true, false, ""};
    if (t->status == TodoStatus::Done) {
        result.warned = true;
        result.message = "Warning: todo " + std::to_string(id) + " is already completed.";
    }
    t->notes.push_back(note);
    return result;
}
