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
