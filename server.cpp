// ======================= TIME-TRAVEL DEBUGGER - SERVER TEMPLATE =======================

// Pipeline this file implements, top to bottom:
//   0. Receive  -- stream the client's .trace bytes straight to source.bin on disk
//   1. Pass 0X0   -- validity check (FUNC/FUNC_END matching)
//   2. Pass 0X1   -- resolve(): copy EVERY source line into resolve.bin as [offset][size][string], then patch CALL targets.
//   3. Pass 0X2   -- execute resolve.bin: tokenize ONE line at a time, update the call stack, take a snapshot -> Timeline
//   4. Pass 0X3   -- serialize Timeline -> session.tdbg(header + snapshot records + dense index)


#include <iostream>
#include <string>
#include <cstdint>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
using namespace std;

// ---- Constants ----
const int32_t MAX_VARS_PER_FRAME = 16;
const int32_t MAX_STACK_DEPTH = 64;
const int32_t MAX_FUNCS = 128;
const int32_t MAX_TOKENS = MAX_VARS_PER_FRAME + 2; // kW + func_name + upto 16 params/args
const int32_t MAX_PATCHES = MAX_FUNCS * 4;
const uint64_t MAX_SOURCE_BYTES = 15ULL * 1024 * 1024; // sanity cap on the declared file length
const int32_t IO_BUFFER_SIZE = 64 * 1024;                  // fixed buffer for streaming to/from disk
const int32_t SOCKET_TIMEOUT_SEC = 5;                      // TODO: apply as SO_RCVTIMEO so a deadclient can't hang the server forever

// ---- Custom data structures

// Stack: back the live Call Stack during execution
template <typename T>
class Stack
{
    struct Node
    {
        T data;
        Node *next;
    };

    Node *top;
    int32_t count;

public:
    Stack()
    {
        top = nullptr;
        count = 0;
    }

    void push(const T &val)
    {
        if (count >= MAX_STACK_DEPTH)
        {
            throw runtime_error("Stack overflow");
        }

        Node *newNode = new Node{val, top};
        top = newNode;
        count++;
    }

    T pop()
    {
        if (isEmpty())
        {
            throw runtime_error("Stack underflow");
        }

        Node *temp = top;
        T value = temp->data;

        top = top->next;
        delete temp;
        count--;

        return value;
    }

    T &peek()
    {
        if (isEmpty())
        {
            throw runtime_error("Stack is empty");
        }

        return top->data;
    }

    bool isEmpty()
    {
        return top == nullptr;
    }

    int32_t depth()
    {
        return count;
    }

    int32_t snapshot_into(T out[], int32_t maxLen)
    {
        if (maxLen <= 0)
        {
            return 0;
        }

        if (out == nullptr)
        {
            throw runtime_error("Invalid snapshot array");
        }

        Node *current = top;
        int32_t copied = 0;

        while (current != nullptr && copied < maxLen)
        {
            out[copied] = current->data;
            copied++;
            current = current->next;
        }

        return copied;
    }

    ~Stack()
    {
        while (!isEmpty())
        {
            pop();
        }
    }

    Stack(const Stack &) = delete;
    Stack &operator=(const Stack &) = delete;
};


// Timeline : doubly linked list of Snapshots
struct Snapshot; // fwd declaration;
struct TimelineNode
{
    Snapshot *data;
    TimelineNode *next;
    TimelineNode *prev;
};
class Timeline
{
    TimelineNode *head, *tail;
    int32_t stepCount;

public:
    Timeline()
    {
        head = nullptr;
        tail = nullptr;
        stepCount = 0;
    }

    ~Timeline();

    Timeline(const Timeline &) = delete;
    Timeline &operator=(const Timeline &) = delete;

    void record(Snapshot *s)
    {
        if (s == nullptr)
        {
            throw runtime_error("Invalid snapshot");
        }

        TimelineNode *newNode = new TimelineNode{s, nullptr, tail};

        if (head == nullptr)
        {
            head = newNode;
        }
        else
        {
            tail->next = newNode;
        }

        tail = newNode;
        stepCount++;
    }

    TimelineNode *begin()
    {
        return head;
    }

    int32_t getStepCount()
    {
        return stepCount;
    }
};


// Core structs
struct Variable
{
    string name;
    int32_t value;
};
struct Frame
{
    string func_name;
    int32_t argc;
    Variable argv[MAX_VARS_PER_FRAME];
    int32_t returnLine;
    Variable locals[MAX_VARS_PER_FRAME];
    int32_t localCount;
};
struct Snapshot
{
    Frame callStack[MAX_STACK_DEPTH];
    int32_t stackDepth;
};

Timeline::~Timeline()
{
    TimelineNode *current = head;

    while (current != nullptr)
    {
        TimelineNode *nextNode = current->next;

        delete current->data;
        delete current;

        current = nextNode;
    }
}

struct TTDBHeader
{
    char magic[4]; // "TTDB"
    int32_t version;
    int32_t stepCount;
    int64_t indexOffset;
};
void writeHeader(FILE *f, const TTDBHeader &h)
{
    fwrite(h.magic, 1, 4, f);
    fwrite(&h.version, sizeof(int32_t), 1, f);
    fwrite(&h.stepCount, sizeof(int32_t), 1, f);
    fwrite(&h.indexOffset, sizeof(int64_t), 1, f);
}

// resolve.bin - bookkeeping
struct FuncEntry
{
    string funcName;
    int64_t byteOffsetInResolveBin; // where this function's FUNC header record sits
};
struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; // where in resolve.bin to seek back and overwrite
    string targetFuncName;
};



// PASS 0x0: READING source.bin + VALIDITY CHECK
bool readSourceLine(ifstream &in, string &out)
{
    while (getline(in, out))
    {
        if (out.find_first_not_of(" \t\r\n") != string::npos)
        {
            return true;
        }
    }

    return false;
}

string firstWord(const string &line)
{
    int i = 0;
    while (i < line.length() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
        i++;

    string word = "";
    while (i < line.length() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
    {
        word += line[i];
        i++;
    }
    return word;
}

string secondWord(const string &line)
{
    int i = 0;
    while (i < line.length() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
        i++;
    while (i < line.length() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
        i++;
    while (i < line.length() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
        i++;

    string word = "";
    while (i < line.length() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
    {
        word += line[i];
        i++;
    }
    return word;
}

bool validateProgram(const char *sourcePath)
{
    ifstream file(sourcePath);

    if (!file.is_open())
    {
        cout << "Cannot open source file" << endl;
        return false;
    }

    string line;
    bool insideFunction = false;
    int32_t totalFunctions = 0;

    while (readSourceLine(file, line))
    {
        string word = firstWord(line);
        if (word == "" || word.substr(0, 2) == "//")
            continue;

        if (word == "func")
        {
            if (insideFunction)
            {
                cout << "Nested function not allowed" << endl;
                return false;
            }

            if (secondWord(line) == "")
            {
                cout << "Function name missing" << endl;
                return false;
            }

            insideFunction = true;
            totalFunctions++;
        }
        else if (word == "func_end")
        {
            if (!insideFunction)
            {
                cout << "Extra func_end found" << endl;
                return false;
            }

            insideFunction = false;
        }
        else if (!insideFunction)
        {
            cout << "Instruction outside function" << endl;
            return false;
        }
    }

    if (insideFunction)
    {
        cout << "Missing func_end" << endl;
        return false;
    }

    if (totalFunctions == 0)
    {
        cout << "No functions found" << endl;
        return false;
    }

    return true;
}

// PASS 0x1: RESOLVE() -> resolve.bin

int64_t writeResolveRecord(FILE *f, int64_t offsetField, const string &text)
{
    int64_t position = ftell(f);
    int32_t size = text.length();

    fwrite(&offsetField, sizeof(int64_t), 1, f);
    fwrite(&size, sizeof(int32_t), 1, f);
    fwrite(text.c_str(), 1, size, f);

    return position;
}

int64_t readResolveRecord(FILE *f, string &outText)
{
    int64_t offset;
    int32_t size;

    if (fread(&offset, sizeof(int64_t), 1, f) != 1)
    {
        return -1;
    }

    if (fread(&size, sizeof(int32_t), 1, f) != 1)
    {
        return -1;
    }

    if (size < 0 || size > 1024 * 1024)
    {
        return -1;
    }

    outText.resize(size);

    if (size > 0 && fread(&outText[0], 1, size, f) != size)
    {
        return -1;
    }

    return offset;
}

int64_t resolveProgram(const char *sourcePath, const char *resolveBinPath)
{
    ifstream file(sourcePath);

    if (!file.is_open())
    {
        cout << "Cannot open source file" << endl;
        return -1;
    }

    FILE *f = fopen(resolveBinPath, "wb+");

    if (f == nullptr)
    {
        cout << "Cannot create resolve file" << endl;
        return -1;
    }

    FuncEntry funcArray[MAX_FUNCS];
    int32_t funcCount = 0;

    PendingPatch patches[MAX_PATCHES];
    int32_t patchCount = 0;

    string line;
    int64_t mainOffset = -1;
    bool valid = true;

    while (getline(file, line))
    {
        string word = firstWord(line);
        string name = secondWord(line);

        int64_t position = writeResolveRecord(f, -1, line);

        if (word == "func")
        {
            if (funcCount >= MAX_FUNCS)
            {
                cout << "Too many functions" << endl;
                valid = false;
                break;
            }

            for (int32_t i = 0; i < funcCount; i++)
            {
                if (funcArray[i].funcName == name)
                {
                    cout << "Duplicate function: " << name << endl;
                    valid = false;
                    break;
                }
            }

            if (!valid)
            {
                break;
            }

            funcArray[funcCount].funcName = name;
            funcArray[funcCount].byteOffsetInResolveBin = position;
            funcCount++;

            if (name == "main")
            {
                mainOffset = position;
            }
        }
        else if (word == "call")
        {
            if (patchCount >= MAX_PATCHES)
            {
                cout << "Too many calls" << endl;
                valid = false;
                break;
            }

            patches[patchCount].byteOffsetOfOffsetField = position;
            patches[patchCount].targetFuncName = name;
            patchCount++;
        }
    }

    if (mainOffset == -1)
    {
        cout << "Main function not found" << endl;
        valid = false;
    }

    for (int32_t i = 0; i < patchCount && valid; i++)
    {
        int64_t target = -1;

        for (int32_t j = 0; j < funcCount; j++)
        {
            if (patches[i].targetFuncName == funcArray[j].funcName)
            {
                target = funcArray[j].byteOffsetInResolveBin;
                break;
            }
        }

        if (target == -1)
        {
            cout << "Undefined function: "
                 << patches[i].targetFuncName << endl;
            valid = false;
            break;
        }

        if (fseek(f, patches[i].byteOffsetOfOffsetField, SEEK_SET) != 0 ||
            fwrite(&target, sizeof(int64_t), 1, f) != 1)
        {
            cout << "Cannot patch function call" << endl;
            valid = false;
            break;
        }
    }

    fclose(f);
    file.close();

    if (!valid)
    {
        remove(resolveBinPath);
        return -1;
    }

    return mainOffset;
}


// PASS 0x2: EXECUTION (tokenization happens here)
enum TokenType
{
    KEYWORD,
    IDENTIFIER,
    PARAM
};
struct Token
{
    TokenType type;
    string text;
};
int32_t tokenizeLine(const string &line, Token tokens[], int32_t maxTokens)
{
    int32_t count = 0;
    int32_t i = 0;

    while (i < line.length())
    {
        while (i < line.length() && (line[i] == ' ' || line[i] == '\t'))
        {
            i++;
        }

        if (i >= line.length())
        {
            break;
        }

        if (count >= maxTokens)
        {
            return -1;
        }

        string word = "";

        while (i < line.length() && line[i] != ' ' && line[i] != '\t')
        {
            word += line[i];
            i++;
        }

        tokens[count].text = word;

        if (count == 0)
        {
            tokens[count].type = KEYWORD;
        }
        else if (count == 1)
        {
            tokens[count].type = IDENTIFIER;
        }
        else
        {
            tokens[count].type = PARAM;
        }

        count++;
    }

    return count;
}
Snapshot *buildSnapshot(Stack<Frame> &callStack)
{
    Snapshot *s = new Snapshot{};

    s->stackDepth = callStack.snapshot_into(s->callStack, MAX_STACK_DEPTH);

    return s;
}
bool isNumber(const string &text)
{
    if (text.empty())
        return false;

    int i = 0;

    if (text[0] == '-' || text[0] == '+')
        i = 1;

    if (i == text.length())
        return false;

    while (i < text.length())
    {
        if (text[i] < '0' || text[i] > '9')
            return false;

        i++;
    }

    return true;
}

Variable *findVariable(Frame &frame, const string &name)
{
    for (int i = 0; i < frame.argc; i++)
    {
        if (frame.argv[i].name == name)
            return &frame.argv[i];
    }

    for (int i = 0; i < frame.localCount; i++)
    {
        if (frame.locals[i].name == name)
            return &frame.locals[i];
    }

    return nullptr;
}

int32_t getValue(Frame &frame, const string &text)
{
    if (isNumber(text))
        return stoi(text);

    Variable *var = findVariable(frame, text);

    if (var == nullptr)
        throw runtime_error("Unknown variable: " + text);

    return var->value;
}

void setValue(Frame &frame, const string &name, int32_t value)
{
    Variable *var = findVariable(frame, name);

    if (var != nullptr)
    {
        var->value = value;
        return;
    }

    if (frame.localCount >= MAX_VARS_PER_FRAME)
        throw runtime_error("Too many variables");

    frame.locals[frame.localCount].name = name;
    frame.locals[frame.localCount].value = value;

    frame.localCount++;
}

void executeProgram(const char *resolveBinPath, int64_t mainOffset, Timeline &timeline)
{
    FILE *file = fopen(resolveBinPath, "rb");

    if (file == nullptr || mainOffset < 0)
        throw runtime_error("Cannot start execution");

    Stack<Frame> callStack;
    string argNames[MAX_STACK_DEPTH][MAX_VARS_PER_FRAME];
    string line;
    Token words[MAX_TOKENS];

    fseek(file, mainOffset, SEEK_SET);
    readResolveRecord(file, line);

    int count = tokenizeLine(line, words, MAX_TOKENS);

    if (count != 2 || words[0].text != "func" || words[1].text != "main")
    {
        fclose(file);
        throw runtime_error("Invalid main function");
    }

    Frame mainFrame{};
    mainFrame.func_name = "main";

    callStack.push(mainFrame);
    timeline.record(buildSnapshot(callStack));

    int64_t pc = ftell(file);
    bool done = false;

    while (!done)
    {
        if (fseek(file, pc, SEEK_SET) != 0)
        {
            fclose(file);
            throw runtime_error("Invalid instruction address");
        }

        line = "";

        int64_t target = readResolveRecord(file, line);
        int64_t next = ftell(file);

        if (next <= pc || feof(file))
        {
            fclose(file);
            throw runtime_error("Unexpected end of program");
        }

        pc = next;

        count = tokenizeLine(line, words, MAX_TOKENS);

        if (count < 0)
        {
            fclose(file);
            throw runtime_error("Too many tokens");
        }

        if (count == 0 || words[0].text[0] == '/')
            continue;

        string op = words[0].text;
        Frame &current = callStack.peek();

        if (op == "set" || op == "add" || op == "sub" ||
            op == "mul" || op == "div")
        {
            if (count != 3)
                throw runtime_error("Wrong number of operands");

            string name = words[1].text;
            int32_t right = getValue(current, words[2].text);
            int32_t result = right;

            if (op != "set")
            {
                int32_t left = getValue(current, name);

                if (op == "add")
                    result = left + right;

                if (op == "sub")
                    result = left - right;

                if (op == "mul")
                    result = left * right;

                if (op == "div")
                {
                    if (right == 0)
                        throw runtime_error("Division by zero");

                    result = left / right;
                }
            }

            setValue(current, name, result);
        }
        else if (op == "call")
        {
            if (count < 2 || target < 0)
                throw runtime_error("Invalid function call");

            if (fseek(file, target, SEEK_SET) != 0)
                throw runtime_error("Invalid call target");

            string header;
            readResolveRecord(file, header);

            Token params[MAX_TOKENS];

            int num = tokenizeLine(header, params, MAX_TOKENS);

            if (num < 2 || params[0].text != "func" ||
                params[1].text != words[1].text)
                throw runtime_error("Function not found");

            if (num - 2 != count - 2)
                throw runtime_error("Wrong number of arguments");

            if (callStack.depth() >= MAX_STACK_DEPTH)
                throw runtime_error("Stack overflow");

            int slot = callStack.depth();

            Frame called{};
            called.func_name = params[1].text;
            called.argc = num - 2;
            called.returnLine = pc;

            for (int i = 0; i < called.argc; i++)
            {
                called.argv[i].name = params[i + 2].text;
                called.argv[i].value = getValue(current, words[i + 2].text);

                argNames[slot][i] = "";

                if (!isNumber(words[i + 2].text))
                    argNames[slot][i] = words[i + 2].text;
            }

            callStack.push(called);
            pc = ftell(file);
        }
        else if (op == "func_end")
        {
            Frame finished = callStack.pop();

            if (callStack.isEmpty())
            {
                done = true;
            }
            else
            {
                int slot = callStack.depth();
                Frame &parent = callStack.peek();

                for (int i = 0; i < finished.argc; i++)
                {
                    if (argNames[slot][i] != "")
                        setValue(parent, argNames[slot][i], finished.argv[i].value);
                }

                pc = finished.returnLine;
            }
        }
        else
        {
            throw runtime_error("Unknown instruction: " + op);
        }

        timeline.record(buildSnapshot(callStack));
    }

    fclose(file);
}

// PASS 0x3: SERIALIZE TIMELINE
bool writeString(FILE *file, const string &text)
{
    int32_t size = text.length();
    if (fwrite(&size, sizeof(int32_t), 1, file) != 1)
        return false;
    if (size > 0 && fwrite(text.c_str(), size, 1, file) != 1)
        return false;
    return true;
}

bool writeVariable(FILE *file, const Variable &v)
{
    if (!writeString(file, v.name))
        return false;
    return fwrite(&v.value, sizeof(int32_t), 1, file) == 1;
}

bool writeFrame(FILE *file, const Frame &frame)
{
    if (frame.argc < 0 || frame.argc > MAX_VARS_PER_FRAME ||
        frame.localCount < 0 || frame.localCount > MAX_VARS_PER_FRAME)
        return false;

    if (!writeString(file, frame.func_name))
        return false;
    if (fwrite(&frame.argc, sizeof(int32_t), 1, file) != 1)
        return false;
    if (fwrite(&frame.returnLine, sizeof(int32_t), 1, file) != 1)
        return false;
    if (fwrite(&frame.localCount, sizeof(int32_t), 1, file) != 1)
        return false;

    for (int i = 0; i < frame.argc; i++)
    {
        if (!writeVariable(file, frame.argv[i]))
            return false;
    }
    for (int i = 0; i < frame.localCount; i++)
    {
        if (!writeVariable(file, frame.locals[i]))
            return false;
    }
    return true;
}

void writeTdbg(Timeline &timeline, const char *tdbgPath)
{
    FILE *file = fopen(tdbgPath, "wb+");
    if (file == nullptr)
        throw runtime_error("Cannot create tdbg file");

    TTDBHeader header{{'T', 'T', 'D', 'B'}, 1, timeline.getStepCount(), 0};
    writeHeader(file, header);

    int32_t steps = timeline.getStepCount();
    int64_t *index = new int64_t[steps];
    TimelineNode *node = timeline.begin();
    bool ok = true;

    for (int i = 0; i < steps; i++)
    {
        if (node == nullptr || node->data == nullptr)
        {
            ok = false;
            break;
        }

        index[i] = ftell(file);
        Snapshot *snap = node->data;
        int32_t depth = snap->stackDepth;

        if (depth < 0 || depth > MAX_STACK_DEPTH ||
            fwrite(&depth, sizeof(int32_t), 1, file) != 1)
        {
            ok = false;
            break;
        }

        for (int j = 0; j < depth; j++)
        {
            if (!writeFrame(file, snap->callStack[j]))
            {
                ok = false;
                break;
            }
        }
        if (!ok)
            break;

        node = node->next;
    }

    if (ok)
    {
        header.indexOffset = ftell(file);
        for (int i = 0; i < steps; i++)
        {
            if (fwrite(&index[i], sizeof(int64_t), 1, file) != 1)
            {
                ok = false;
                break;
            }
        }
    }

    if (ok && fseek(file, 0, SEEK_SET) == 0)
    {
        writeHeader(file, header);
    }
    else
    {
        ok = false;
    }

    delete[] index;
    if (fclose(file) != 0)
        ok = false;

    if (!ok)
    {
        remove(tdbgPath);
        throw runtime_error("Cannot write tdbg file");
    }
}

int32_t main()
{
    remove("session.tdbg");
    if (!validateProgram("source.bin"))
        return 1;

    int64_t mainOffset = resolveProgram("source.bin", "resolve.bin");
    if (mainOffset < 0)
        return 1;

    try
    {
        Timeline timeline;
        executeProgram("resolve.bin", mainOffset, timeline);
        writeTdbg(timeline, "session.tdbg");
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        remove("session.tdbg");
        return 1;
    }

    cout << "Created session.tdbg" << endl;
    return 0;
}
