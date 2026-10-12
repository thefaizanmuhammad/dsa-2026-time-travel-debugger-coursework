#define main server_main
#include "server.cpp"
#undef main

#include <cassert>

int main()
{
    ofstream file("valid_resolve.trace");

    file << "func main\n";
    file << "call foo 7\n";
    file << "func_end\n";
    file << "func foo x\n";
    file << "set x 4\n";
    file << "func_end\n";

    file.close();

    assert(validateProgram("valid_resolve.trace"));

    int64_t mainOffset = resolveProgram("valid_resolve.trace", "resolve.bin");

    assert(mainOffset == 0);

    FILE *f = fopen("resolve.bin", "rb");
    assert(f != nullptr);

    string line;

    readResolveRecord(f, line);
    assert(line == "func main");

    int64_t callOffset = readResolveRecord(f, line);
    assert(line == "call foo 7");

    readResolveRecord(f, line);
    assert(line == "func_end");

    int64_t fooOffset = ftell(f);

    readResolveRecord(f, line);
    assert(line == "func foo x");

    assert(callOffset == fooOffset);

    fclose(f);

    ofstream file2("missing_main.trace");
    file2 << "func foo\nfunc_end\n";
    file2.close();

    assert(resolveProgram("missing_main.trace", "resolve.bin") == -1);

    ofstream file3("undefined.trace");
    file3 << "func main\ncall abc\nfunc_end\n";
    file3.close();

    assert(resolveProgram("undefined.trace", "resolve.bin") == -1);

    ofstream file4("duplicate.trace");
    file4 << "func main\nfunc_end\n";
    file4 << "func main\nfunc_end\n";
    file4.close();

    assert(resolveProgram("duplicate.trace", "resolve.bin") == -1);

    cout << "All resolver tests passed!" << endl;

    return 0;
}