# Memory File System

A simplified in-memory file system implemented in C, supporting files, directories, symbolic links, path resolution, and recursive file system operations.

## Features

- File and directory creation
- Symbolic link support
- Relative and absolute path resolution
- Current working directory management
- File read and write operations
- Directory traversal and recursive deletion
- Dynamic memory allocation and cleanup

## Technologies

- C
- Makefile
- Linked list data structures

## Project Structure

```text
.
├── Fs.c
├── Fs.h
├── Path.c
├── Path.h
├── Makefile
└── README.md
```

## Build

Compile the project using:

```bash
make
```

## Highlights

- Implemented a tree-structured in-memory file system
- Designed path traversal and symbolic link resolution logic
- Managed recursive directory operations and memory cleanup in C
- Simulated Unix-like filesystem behaviours including `mkdir`, `cd`, `ls`, `pwd`, and symbolic links