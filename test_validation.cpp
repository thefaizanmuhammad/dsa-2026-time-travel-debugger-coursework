#define main server_main
#include "server.cpp"
#undef main

#include <cassert>

void createFile(string name, string content)
{
    ofstream file(name);
    file << content;
    file.close();
}

int main()
{
    createFile("valid.trace",
        "func foo a\n"
        "set a 10\n"
        "func_end\n"
        "func main\n"
        "call foo 5\n"
        "func_end\n");

    assert(validateProgram("valid.trace"));

    createFile("missing_end.trace",
        "func main\n"
        "set a 10\n");

    assert(!validateProgram("missing_end.trace"));

    createFile("nested.trace",
        "func main\n"
        "func foo\n"
        "func_end\n"
        "func_end\n");

    assert(!validateProgram("nested.trace"));

    createFile("extra_end.trace",
        "func main\n"
        "func_end\n"
        "func_end\n");

    assert(!validateProgram("extra_end.trace"));

    createFile("missing_name.trace",
        "func\n"
        "func_end\n");

    assert(!validateProgram("missing_name.trace"));

    createFile("no_main.trace",
        "func foo\n"
        "func_end\n");

    assert(validateProgram("no_main.trace"));

    createFile("outside.trace",
        "set a 10\n"
        "func main\n"
        "func_end\n");

    assert(!validateProgram("outside.trace"));

    cout << "All validation tests passed!" << endl;

    return 0;
}