#define main server_main
#include "server.cpp"
#undef main

#include <cassert>

int main()
{
    Stack<int> s;

    assert(s.isEmpty());
    assert(s.depth() == 0);

    s.push(10);
    s.push(20);
    s.push(30);

    assert(s.depth() == 3);
    assert(s.peek() == 30);

    int values[3] = {};
    assert(s.snapshot_into(values, 3) == 3);
    assert(values[0] == 30);
    assert(values[1] == 20);
    assert(values[2] == 10);

    assert(s.pop() == 30);
    assert(s.pop() == 20);
    assert(s.pop() == 10);
    assert(s.isEmpty());

    bool caught = false;

    try
    {
        s.pop();
    }
    catch (const runtime_error &)
    {
        caught = true;
    }

    assert(caught);

    Timeline timeline;

    Snapshot *first = new Snapshot{};
    Snapshot *second = new Snapshot{};

    first->stackDepth = 1;
    second->stackDepth = 2;

    timeline.record(first);
    timeline.record(second);

    assert(timeline.getStepCount() == 2);

    TimelineNode *node = timeline.begin();

    assert(node != nullptr);
    assert(node->prev == nullptr);
    assert(node->data == first);
    assert(node->next != nullptr);
    assert(node->next->prev == node);
    assert(node->next->data == second);
    assert(node->next->next == nullptr);

    cout << "All Stack and Timeline tests passed!" << endl;

    return 0;
}