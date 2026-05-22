#ifndef PATH_H
#define PATH_H

#include <stdbool.h>

typedef struct path *Path;

// Parses a path string and returns a Path structure representing the path
// Special components like "." or ".." are not handled separately
Path PathParse(char *pathStr);

// Frees a Path structure
void PathFree(Path p);

// Returns the length of a path
int PathLength(Path p);

// Returns whether a path is relative (false) or absolute (true)
bool PathIsAbsolute(Path p);

// Returns the nth component of a path, or NULL if the component does not exist
// Components are zero-indexed, and negative values are interpreted as relative
// to the end (e.g. -1 is the last component, -2 is the second-last)
char *PathComponent(Path p, int n);

// Returns a copy of the path string originally passed into PathParse
char *PathString(Path p);

#endif