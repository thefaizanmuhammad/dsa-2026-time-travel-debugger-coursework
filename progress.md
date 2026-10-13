# DSA 2026 — Time-Travel Debugger

## Project Progress

This file records the daily progress of **Project 01: Time-Travel Debugger**.

---

## 03 October 2026

### Status
Project setup started.

### Completed
- Reviewed the Phase 01 project specification.
- Reviewed the TA-provided `server.cpp` starter code.
- Identified the four main pipeline stages:
  - Pass 0x0 — Validation
  - Pass 0x1 — Resolve
  - Pass 0x2 — Execution
  - Pass 0x3 — Serialization
- Created the GitHub repository.
- Set up the local project repository.

### Currently Working On
- Setting up the starter project files.
- Understanding the custom `Stack` and `Timeline` data structures before implementation.

### Next
- Implement and test the custom `Stack`.
- Implement and test the doubly linked `Timeline`.
- Begin Pass 0x0 validation.

### Notes
- Only built-in `vector` is allowed.
- Any other required data structure will be implemented from scratch.
- Generated files such as `source.bin`, `resolve.bin`, and `session.tdbg` will not be committed unless specifically required for testing/submission.

---

## 13 October 2026

### Status
Core data structures implemented and tested successfully.

### Completed
- Verified the Ubuntu C++ development environment.
- Implemented the custom Stack using a singly linked list.
- Implemented push, pop, peek, isEmpty, depth, and snapshot_into.
- Added stack overflow and underflow handling.
- Implemented the Timeline using a doubly linked list.
- Implemented record, begin, and getStepCount.
- Added memory cleanup through destructors.
- Created independent unit tests for Stack and Timeline.

### Currently Working On
- Pass 0x0: Source program validation.

### Next
- Implement source line reading and function structure validation.
- Test valid and invalid function declarations.
- Begin Pass 0x1: Function resolution.

### Problems / Questions
- Remaining starter functions are incomplete and generate compiler warnings.

### Tests Performed
- Compiled server.cpp using G++ with C++17.
- Confirmed compiler exit code 0.
- Verified Stack and Timeline unit tests passed.

---

## 13 October 2026 — Pass 0x0 Validation

### Status
Source validation implemented and tested.

### Completed
- Implemented readSourceLine, firstWord and secondWord.
- Implemented validateProgram using a Boolean state variable.
- Added checks for nested functions, missing func_end, extra func_end and missing function names.
- Added checks for instructions outside functions.
- Created test_validation.cpp.

### Currently Working On
- Pass 0x1: Function resolution.

### Next
- Implement resolve.bin record writing and reading.
- Implement function offset tracking and CALL patching.
- Test function resolution.

### Problems / Questions
- Remaining pipeline functions are incomplete.

### Tests Performed
- Compiled validation tests using G++ and C++17.
- Valid and invalid program tests passed.
- git diff --check passed.

---

## Daily Update Template

### DD Month YYYY

#### Status
Brief description of the current project state.

#### Completed
-

#### Currently Working On
-

#### Next
-

#### Problems / Questions
- None currently.

#### Tests Performed
-

## 13 October 2026 — Pass 0x1 Resolution

### Status
Function resolver implemented and tested.

### Completed
- Implemented binary record writing and reading.
- Stored function names and their byte offsets.
- Implemented CALL target patching.
- Added checks for missing main, duplicate functions and undefined calls.
- Created resolver tests.

### Currently Working On
- Pass 0x2: Execution engine.

### Next
- Implement tokenization.
- Execute C-- instructions.
- Manage function calls and snapshots.

### Tests Performed
- Compiled resolver using G++ with C++17.
- Verified function offsets and CALL patching.
- Tested missing main, undefined calls and duplicate functions.

## 13 October 2026 — Execution Engine and Serialization

### Status
Phase 01 server pipeline implemented and tested.

### Completed
- Implemented the tokenizer, variable handling, function calls and returns, and snapshot creation.
- Added whitespace handling for function declarations and allowed comment lines.
- Added clean error handling for failed resolution and execution.
- Implemented the `.tdbg` header, snapshot records, and dense index.
- Documented the binary file layout and test commands in README.md.
- Added test_tdbg.cpp to verify the file layout and error handling.

### Tests Performed
- All five existing test files compiled and passed.
- A valid program generated session.tdbg successfully.
- Read back the header, all snapshot records, and all dense-index offsets.
- Verified a function argument changed from 10 to 20 and was returned to the caller.
- Checked invalid calls return a nonzero exit code without leaving a stale session.tdbg file.
- AddressSanitizer and UndefinedBehaviorSanitizer completed the integration test successfully.

### Next
- Repeat the tests on the submission machine.
- Check git diff and push the verified serialization commit.
- Review evaluation questions and be ready to explain the binary format and call stack.

### Notes
- The server is Phase 01 only; the client-side viewer and network receiving are not implemented.
- The current argument behavior copies updated values back when a function returns.
