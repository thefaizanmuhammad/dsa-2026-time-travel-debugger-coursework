#define main server_main
#include "server.cpp"
#undef main

#include <cassert>

int main()
{
    Token tokens[MAX_TOKENS];

    int32_t count = tokenizeLine("call foo a 10", tokens, MAX_TOKENS);

    assert(count == 4);
    assert(tokens[0].text == "call");
    assert(tokens[0].type == KEYWORD);
    assert(tokens[1].text == "foo");
    assert(tokens[1].type == IDENTIFIER);
    assert(tokens[2].text == "a");
    assert(tokens[2].type == PARAM);
    assert(tokens[3].text == "10");

    Stack<Frame> s;

    Frame mainFrame{};
    mainFrame.func_name = "main";

    Frame fooFrame{};
    fooFrame.func_name = "foo";

    s.push(mainFrame);
    s.push(fooFrame);

    Snapshot *snap = buildSnapshot(s);

    assert(snap->stackDepth == 2);
    assert(snap->callStack[0].func_name == "foo");
    assert(snap->callStack[1].func_name == "main");

    s.pop();

    assert(snap->stackDepth == 2);

    delete snap;

    cout << "Execution helper tests passed!" << endl;

    return 0;
}
