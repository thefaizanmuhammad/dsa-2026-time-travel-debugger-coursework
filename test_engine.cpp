#define main server_main
#include "server.cpp"
#undef main

#include <cassert>

int main()
{
    ofstream file("engine.trace");

    file << "func foo b\n";
    file << "set a 10\n";
    file << "add b a\n";
    file << "func_end\n";
    file << "func main\n";
    file << "set k 10\n";
    file << "call foo k\n";
    file << "func_end\n";

    file.close();

    assert(validateProgram("engine.trace"));

    int64_t start = resolveProgram("engine.trace", "resolve.bin");

    assert(start >= 0);

    Timeline timeline;

    executeProgram("resolve.bin", start, timeline);

    assert(timeline.getStepCount() == 7);

    TimelineNode *node = timeline.begin();

    bool foundFoo = false;
    bool foundMain = false;

    while (node != nullptr)
    {
        Snapshot *snap = node->data;

        if (snap->stackDepth == 2)
        {
            Frame &f = snap->callStack[0];

            for (int i = 0; i < f.argc; i++)
            {
                if (f.argv[i].name == "b" && f.argv[i].value == 20)
                {
                    foundFoo = true;
                }
            }
        }

        if (snap->stackDepth == 1)
        {
            Frame &f = snap->callStack[0];

            for (int i = 0; i < f.localCount; i++)
            {
                if (f.locals[i].name == "k" && f.locals[i].value == 20)
                {
                    foundMain = true;
                }
            }
        }

        node = node->next;
    }

    assert(foundFoo);
    assert(foundMain);

    cout << "Execution engine tests passed!" << endl;

    return 0;
}