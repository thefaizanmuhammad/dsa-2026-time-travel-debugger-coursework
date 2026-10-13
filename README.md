# DSA 2026 — Time-Travel Debugger

Project 01 for Data Structures and Algorithms (Phase 01 — Server).

The server takes an existing `source.bin` file containing instructions in the project's C-- language. It checks the program, resolves function calls, executes it, records snapshots, and creates `session.tdbg`.

## Pipeline

1. Pass 0x0: validate `func` and `func_end` declarations.
2. Pass 0x1: create `resolve.bin` and patch CALL destination offsets.
3. Pass 0x2: execute from `main` using a custom linked-list Stack and doubly linked Timeline.
4. Pass 0x3: write the header, snapshots, and dense index to `session.tdbg`.

## Build and run on Ubuntu

```bash
g++ -std=c++17 server.cpp -o server
./server
```

`source.bin` must exist in the current directory. On success, the program creates `resolve.bin` and `session.tdbg`. A missing or invalid function produces an error and a nonzero exit code.

## Tests

```bash
g++ -std=c++17 -Werror=return-type test_structures.cpp -o test_structures && ./test_structures
g++ -std=c++17 -Werror=return-type test_validation.cpp -o test_validation && ./test_validation
g++ -std=c++17 -Werror=return-type test_resolve.cpp -o test_resolve && ./test_resolve
g++ -std=c++17 -Werror=return-type test_execution.cpp -o test_execution && ./test_execution
g++ -std=c++17 -Werror=return-type test_engine.cpp -o test_engine && ./test_engine
g++ -std=c++17 -Werror=return-type test_tdbg.cpp -o test_tdbg && ./test_tdbg
```

## `.tdbg` file layout (version 1)

The header has four fields in this order: `TTDB` (4 bytes), `version` (4 bytes), `stepCount` (4 bytes), and `indexOffset` (8 bytes).

After the header, each snapshot stores `stackDepth` (4 bytes), followed by the active frames from top to bottom. Each frame stores the function name, argument count, return line, local count, arguments, and locals. Strings are written as their length (`int32_t`) followed by the characters; each variable includes a name and an `int32_t` value. Only active frames and variables are written, because `std::string` objects cannot safely be written as raw C++ objects.

At `indexOffset`, the file contains `stepCount` consecutive `int64_t` offsets. Entry `i` identifies the start of snapshot `i`. Integer fields use the host machine's native byte order; the current build targets Ubuntu on x86-64.

## Notes

The starter's Phase 01 assumes `source.bin` is already present, so socket receiving and the Phase 02 client are outside this deliverable. Function arguments are copied into the called frame and copied back to the caller when the function returns. Execution snapshots are stored after executed instructions, including the initial `main` frame.
