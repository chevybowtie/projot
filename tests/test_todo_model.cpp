#include "doctest.h"
#include "todo.h"

static std::vector<Todo> make_todos() {
    return {
        {1, "Todo one",   false, "2025-01-01", "", {}},
        {2, "Todo two",   true,  "2025-01-01", "2025-01-03", {}},
        {3, "Todo three", false, "2025-01-02", "", {"note"}}
    };
}

TEST_CASE("add_first_todo") {
    std::vector<Todo> todos;
    CHECK(next_todo_id(todos) == 1);
}

TEST_CASE("add_second_todo") {
    std::vector<Todo> todos = {{1, "First", false, "2025-01-01", "", {}}};
    CHECK(next_todo_id(todos) == 2);
}

TEST_CASE("add_after_gap") {
    std::vector<Todo> todos = {
        {1, "One", false, "", "", {}},
        {3, "Three", false, "", "", {}}
    };
    CHECK(next_todo_id(todos) == 4);
}

TEST_CASE("complete_todo") {
    auto todos = make_todos();
    auto result = complete_todo(todos, 1, "2025-02-01");
    CHECK(result.ok);
    CHECK_FALSE(result.warned);
    CHECK(todos[0].completed);
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
    CHECK(todos[1].completed);
    CHECK_FALSE(todos[0].completed);
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
    for (const auto* t : open) CHECK_FALSE(t->completed);
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
