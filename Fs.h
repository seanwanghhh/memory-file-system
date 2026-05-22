#ifndef FS_H
#define FS_H

#define PATH_MAX 4096

typedef enum {
    REGULAR_FILE,
    DIRECTORY,
    SYMBOLIC
} FileType;

typedef struct FsRep *Fs;

// Returns a new, empty filesystem, with only a root directory
Fs FsNew(void);

// Populates cwd with the canonical path of the current working directory of
// the filesystem
// Can assume than the canonical path is at most PATH_MAX characters
void FsGetCwd(Fs fs, char cwd[PATH_MAX + 1]);

// Frees all memory associated with the given filesystem
void FsFree(Fs fs);

// Creates a new, empty directory at the given path in the filesystem
void FsMkdir(Fs fs, char *path);

// Creates a new, empty file at the given path in the filesystem
void FsMkfile(Fs fs, char *path);

// Creates a new symbolic link at the given path in the filesystem which refers
// to the path ref
void FsMklink(Fs fs, char *path, char *ref);

// Changes the filesystem's current working directory to the given path
void FsCd(Fs fs, char *path);

// Lists the non-hidden files which are contained within the directory provided
// by the given path, or the path itself if it refers to a regular file
void FsLs(Fs fs, char *path);

// Prints the current working directory of the given filesystem
void FsPwd(Fs fs);

// Prints out a tree-like representation of the filesystem, starting at the
// given path, to the given depth limit and optionally expanding symbolic links
void FsTree(Fs fs, char *path, int limit, bool expand);

// Puts the given content in the file referred to by the given path in the
// filesystem if it is a regular file
void FsPut(Fs fs, char *path, char *content);

// Prints the contents in the file referred to by the given path in the
// filesyetem if it is a regular file
void FsCat(Fs fs, char *path);

// Deletes the directory referred to by the given path in the filesystem if it
// is empty
void FsDldir(Fs fs, char *path);

// Deletes the file referred to by the given path in the filesystem
// Non-empty directories are only deleted if recursive is true
void FsDl(Fs fs, bool recursive, char *path);

// Returns the memory usage of the sub-filesystem referred to by the given path
// to the given depth limit, optionally expanding symbolic links
int FsMu(Fs fs, char *path, int limit, bool expand);

#endif