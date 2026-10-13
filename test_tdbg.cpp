#define main server_main
#include "server.cpp"
#undef main

#include <cassert>

string readString(FILE *file)
{
    int32_t size = 0;
    assert(fread(&size, sizeof(int32_t), 1, file) == 1);
    assert(size >= 0 && size < 10000);
    string result(size, ' ');
    if (size > 0)
        assert(fread(&result[0], 1, size, file) == size);
    return result;
}

void readFrame(FILE *file, Frame &frame)
{
    frame.func_name = readString(file);
    assert(fread(&frame.argc, sizeof(int32_t), 1, file) == 1);
    assert(fread(&frame.returnLine, sizeof(int32_t), 1, file) == 1);
    assert(fread(&frame.localCount, sizeof(int32_t), 1, file) == 1);
    assert(frame.argc >= 0 && frame.argc <= MAX_VARS_PER_FRAME);
    assert(frame.localCount >= 0 && frame.localCount <= MAX_VARS_PER_FRAME);

    for (int i = 0; i < frame.argc; i++)
    {
        frame.argv[i].name = readString(file);
        assert(fread(&frame.argv[i].value, sizeof(int32_t), 1, file) == 1);
    }
    for (int i = 0; i < frame.localCount; i++)
    {
        frame.locals[i].name = readString(file);
        assert(fread(&frame.locals[i].value, sizeof(int32_t), 1, file) == 1);
    }
}

int main()
{
    ofstream source("source.bin");
    source << "  // sample program\n";
    source << "  func foo b\n";
    source << "  set a 10\n";
    source << "  add b a\n";
    source << "  func_end\n";
    source << "\tfunc main\n";
    source << "  set k 10\n";
    source << "  call foo k\n";
    source << "  func_end\n";
    source.close();

    assert(server_main() == 0);

    FILE *file = fopen("session.tdbg", "rb");
    assert(file != nullptr);
    char magic[4] = {};
    int32_t version = 0, steps = 0;
    int64_t indexOffset = 0;
    assert(fread(magic, 1, 4, file) == 4);
    assert(fread(&version, sizeof(int32_t), 1, file) == 1);
    assert(fread(&steps, sizeof(int32_t), 1, file) == 1);
    assert(fread(&indexOffset, sizeof(int64_t), 1, file) == 1);
    assert(magic[0] == 'T' && magic[1] == 'T' && magic[2] == 'D' && magic[3] == 'B');
    assert(version == 1 && steps == 7 && indexOffset > 20);

    int64_t offsets[16];
    assert(fseek(file, indexOffset, SEEK_SET) == 0);
    for (int i = 0; i < steps; i++)
        assert(fread(&offsets[i], sizeof(int64_t), 1, file) == 1);
    assert(ftell(file) == indexOffset + steps * 8);
    assert(fseek(file, 0, SEEK_END) == 0);
    assert(ftell(file) == indexOffset + steps * 8);

    bool foundFoo = false, foundMain = false;
    for (int i = 0; i < steps; i++)
    {
        assert(fseek(file, offsets[i], SEEK_SET) == 0);
        int32_t depth = 0;
        assert(fread(&depth, sizeof(int32_t), 1, file) == 1);
        assert(depth >= 0 && depth <= 2);

        for (int j = 0; j < depth; j++)
        {
            Frame frame{};
            readFrame(file, frame);
            if (frame.func_name == "foo")
                for (int k = 0; k < frame.argc; k++)
                    if (frame.argv[k].name == "b" && frame.argv[k].value == 20)
                        foundFoo = true;
            if (frame.func_name == "main")
                for (int k = 0; k < frame.localCount; k++)
                    if (frame.locals[k].name == "k" && frame.locals[k].value == 20)
                        foundMain = true;
        }
        if (i + 1 < steps)
            assert(ftell(file) == offsets[i + 1]);
        else
            assert(ftell(file) == indexOffset);
    }
    assert(foundFoo && foundMain);
    fclose(file);

    ofstream bad("source.bin");
    bad << "func main\ncall missing\nfunc_end\n";
    bad.close();
    assert(server_main() == 1);
    assert(fopen("session.tdbg", "rb") == nullptr);

    cout << "Serialization and error handling tests passed!" << endl;
    return 0;
}
