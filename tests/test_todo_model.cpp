#include "doctest.h"
#include "todo.h"

static std::vector<Todo> make_todos() {
    return {
        {1, "Todo one",   TodoStatus::Todo, "2025-01-01", "", {}},
        {2, "Todo two",   TodoStatus::Done, "2025-01-01", "2025-01-03", {}},
        {3, "Todo three", TodoStatus::Todo, "2025-01-02", "", {"note"}}
    };
}

TEST_CASE("add_first_todo") {
    std::vector<Todo> todos;
    CHECK(next_todo_id(todos) == 1);
}

TEST_CASE("add_second_todo") {
    std::vector<Todo> todos = {{1, "First", TodoStatus::Todo, "2025-01-01", "", {}}};
    CHECK(next_todo_id(todos) == 2);
}

TEST_CASE("add_after_gap") {
    std::vector<Todo> todos = {
        {1, "One",   TodoStatus::Todo, "", "", {}},
        {3, "Three", TodoStatus::Todo, "", "", {}}
    };
    CHECK(next_todo_id(todos) == 4);
}

TEST_CASE("complete_todo") {
    auto todos = make_todos();
    auto result = complete_todo(todos, 1, "2025-02-01");
    CHECK(result.ok);
    CHECK_FALSE(result.warned);
    CHECK(todos[0].status == TodoStatus::Done);
    CHECK(todos[0].completed_date == "2025-02-01");
}

TEST_CASE("complete_already_done") {
    auto todos = make_todos();
    auto result = complete_todo(todos, 2, "2025-02-01");
    CHECK(result.ok);
    CHECK(result.warned);
    // State unchanged
    CHECK(todos[1].completed_date == "2025-01-03");
}

TEST_CASE("add_note_to_open") {
    auto todos = make_todos();
    auto result = add_note(todos, 1, "New note");
    CHECK(result.ok);
    CHECK_FALSE(result.warned);
    CHECK(todos[0].notes.size() == 1);
    CHECK(todos[0].notes[0] == "New note");
}

TEST_CASE("add_note_to_closed") {
    auto todos = make_todos();
    auto result = add_note(todos, 2, "Late note");
    CHECK(result.ok);
    CHECK(result.warned);
    CHECK(todos[1].notes.size() == 1);
}

TEST_CASE("id_stability_after_complete") {
    auto todos = make_todos();
    complete_todo(todos, 2, "2025-02-01");
    REQUIRE(todos.size() == 3);
    CHECK(todos[0].id == 1);
    CHECK(todos[1].id == 2);
    CHECK(todos[2].id == 3);
    CHECK(todos[1].status == TodoStatus::Done);
    CHECK(todos[0].status == TodoStatus::Todo);
}

TEST_CASE("find_by_id_valid") {
    auto todos = make_todos();
    const auto* t = find_todo(todos, 2);
    REQUIRE(t != nullptr);
    CHECK(t->text == "Todo two");
}

TEST_CASE("find_by_id_missing") {
    auto todos = make_todos();
    const auto* t = find_todo(todos, 99);
    CHECK(t == nullptr);
}

TEST_CASE("list_open") {
    auto todos = make_todos();
    auto open = filter_todos(todos, TodoFilter::Open);
    REQUIRE(open.size() == 2);
    for (const auto* t : open) CHECK(t->status != TodoStatus::Done);
}

TEST_CASE("list_closed") {
    auto todos = make_todos();
    auto closed = filter_todos(todos, TodoFilter::Closed);
    REQUIRE(closed.size() == 1);
    CHECK(closed[0]->id == 2);
}

TEST_CASE("list_all") {
    auto todos = make_todos();
    auto all = filter_todos(todos, TodoFilter::All);
    CHECK(all.size() == 3);
}

TEST_CASE("set_todo_status_in_progress") {
    auto todos = make_todos();
    auto result = set_todo_status(todos, 1, TodoStatus::InProgress, "2025-03-01");
    CHECK(result.ok);
    CHECK_FALSE(result.warned);
    CHECK(todos[0].status == TodoStatus::InProgress);
    CHECK(todos[0].completed_date.empty()); // not done; no completed date
}

TEST_CASE("set_todo_status_blocked") {
    auto todos = make_todos();
    auto result = set_todo_status(todos, 1, TodoStatus::Blocked, "2025-03-01");
    CHECK(result.ok);
    CHECK_FALSE(result.warned);
    CHECK(todos[0].status == TodoStatus::Blocked);
    CHECK(todos[0].completed_date.empty());
}

TEST_CASE("set_todo_status_done_sets_date") {
    auto todos = make_todos();
    auto result = set_todo_status(todos, 1, TodoStatus::Done, "2025-03-15");
    CHECK(result.ok);
    CHECK_FALSE(result.warned);
    CHECK(todos[0].status == TodoStatus::Done);
    CHECK(todos[0].completed_date == "2025-03-15");
}

TEST_CASE("set_todo_status_clears_date_when_reopened") {
    auto todos = make_todos();
    // Start: todo 2 is Done with completed_date
    CHECK(todos[1].status == TodoStatus::Done);
    CHECK(!todos[1].completed_date.empty());
    auto result = set_todo_status(todos, 2, TodoStatus::Todo, "");
    CHECK(result.ok);
    CHECK(todos[1].status == TodoStatus::Todo);
    CHECK(todos[1].completed_date.empty());
}

TEST_CASE("set_todo_status_already_at_status_warns") {
    auto todos = make_todos();
    auto result = set_todo_status(todos, 1, TodoStatus::Todo, "");
    CHECK(result.ok);
    CHECK(result.warned);
    CHECK(todos[0].status == TodoStatus::Todo); // unchanged
}

TEST_CASE("set_todo_status_missing_id") {
    auto todos = make_todos();
    auto result = set_todo_status(todos, 99, TodoStatus::InProgress, "2025-03-01");
    CHECK_FALSE(result.ok);
}
