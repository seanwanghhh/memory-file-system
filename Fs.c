#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Fs.h"
#include "Path.h"

typedef struct Node {
    char *name;             // name of file/dir
    char *content;          // content for regular files
    char *symbolic;         // target path for symbolic links
    FileType type;
    struct Node *parent;    // parent directory
    struct Node *children;  // first child in linked list
    struct Node *next;
} Node;

struct FsRep {
    Node *root;
    Node *cwd;
    char cwdPath[PATH_MAX + 1];
};

static void freeNodeList(Node *n);
static Node *newNode(char *name, FileType type, Node *parent);
static Node *findChild(Node *dir, const char *name);
static Node *edgeTest(Fs fs, Path p, int stop, bool follow_sym, 
                    const char *cmd_1, const char *path);
static void insertChild(Node *parent, Node *child);
static void treePrint(Node *n, int depth);
static Node *resolveSym(Fs fs, Node *link);
static Node *canonical(Fs fs, Node *canon);

Fs FsNew(void) {
    Fs fs = malloc(sizeof(*fs));
    if (fs == NULL) return NULL;
    fs->root = newNode("/", DIRECTORY, NULL);
    if (fs->root == NULL) {
        free(fs);
        return NULL;
    }
    fs->cwd = fs->root;
    strcpy(fs->cwdPath, "/");
    return fs;
}

void FsGetCwd(Fs fs, char cwd[PATH_MAX + 1]) {
    if (fs == NULL) {
        strcpy(cwd, "/");
        return;
    }
    strcpy(cwd, fs->cwdPath);
}

void FsFree(Fs fs) {
    if (fs == NULL) return;
    freeNodeList(fs->root);
    free(fs);
}

void FsMkdir(Fs fs, char *path) {
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);

    if (len == 0) {
        printf("mkdir: cannot create directory '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    Node *curr = edgeTest(fs, p, len - 1, true, "mkdir: cannot create directory", path);
    if (curr == NULL) {
        PathFree(p);
        return;
    }

    // get the name of the new directory
    char *newName = PathComponent(p, len - 1);
    if (strcmp(newName, ".") == 0 || strcmp(newName, "..") == 0) {
        printf("mkdir: cannot create directory '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    // check if name already exists
    if (findChild(curr, newName) != NULL) {
        printf("mkdir: cannot create directory '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    // create and initialise new directory node
    Node *n = malloc(sizeof(*n));
    n->name = strdup(newName);
    n->type = DIRECTORY;
    n->parent = curr;
    n->symbolic = NULL;
    n->children = NULL;
    n->next = NULL;
    n->content = NULL;
    insertChild(curr, n);
    PathFree(p);
}

void FsMkfile(Fs fs, char *path) {
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);

    // cant create file at root
    if (len == 0) {
        printf("mkfile: cannot create file '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    Node *curr = edgeTest(fs, p, len - 1, true, "mkfile: cannot create file", path);
    if (curr == NULL) {
        PathFree(p);
        return;
    }

    char *newName = PathComponent(p, len - 1);
    if (strcmp(newName, ".") == 0 || strcmp(newName, "..") == 0) {
        printf("mkfile: cannot create file '%s': File exists\n", path);
        PathFree(p);
        return;
    }
    // check if name conflicts with existing file
    if (findChild(curr, newName) != NULL) {
        printf("mkfile: cannot create file '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    // create new file node
    Node *n = malloc(sizeof(*n));
    n->name = strdup(newName);
    n->type = REGULAR_FILE;
    n->parent = curr;
    n->symbolic = NULL;
    n->children = NULL;
    n->next = NULL;
    n->content = NULL;

    insertChild(curr, n);
    PathFree(p);
}

void FsMklink(Fs fs, char *path, char *ref) {
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);
    if (len == 0) {
        printf("mklink: failed to create symbolic link '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    Node *curr = edgeTest(fs, p, len - 1, true, "mklink: failed to create symbolic link\n", path);
    if (curr == NULL) {
        PathFree(p);
        return;
    }

    char *name = PathComponent(p, len - 1);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("mklink: failed to create symbolic link '%s': File exists\n", path);
        PathFree(p);
        return;
    }
    // ensure no name collision
    if (findChild(curr, name) != NULL) {
        printf("mklink: failed to create symbolic link '%s': File exists\n", path);
        PathFree(p);
        return;
    }

    // create symlink node
    Node *n = malloc(sizeof(*n));
    n->name = strdup(name);
    n->type = SYMBOLIC;
    n->symbolic = strdup(ref);
    n->content = NULL;
    n->children = NULL;
    n->parent = curr;
    n->next = NULL;

    insertChild(curr, n);
    PathFree(p);
}

void FsCd(Fs fs, char *path) {
    if (path == NULL) {
        fs->cwd = fs->root;
        strcpy(fs->cwdPath, "/");
        return;
    }

    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);
    // canon tracks logical path
    // real follows physical path
    Node *canon = PathIsAbsolute(p) ? fs->root : fs->cwd;
    Node *real = canonical(fs, canon);
    if (real == NULL) {
        if (canon == fs->root) {
            real = fs->root;
        } else {
            real = canon;
        }
    }

    // build new logical path string
    char newPath[PATH_MAX + 1];
    if (PathIsAbsolute(p)) {
        strcpy(newPath, "");
    } else {
        strcpy(newPath, fs->cwdPath);
    }

    for (int i = 0; i < len; i++) {
        char *comp = PathComponent(p, i);

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            // move up in logical path
            if (canon->parent != NULL) {
                canon = canon->parent;
                // remove last component
                char *lastSlash = strrchr(newPath, '/');
                if (lastSlash != NULL && lastSlash != newPath) {
                    *lastSlash = '\0';
                } else {
                    strcpy(newPath, "");
                }
            }
            // update real to match new canon
            real = canonical(fs, canon);

            if (real == NULL) {
                printf("cd: '%s': No such file or directory\n", path);
                PathFree(p);
                return;
            }
            continue;
        }

        Node *child = findChild(real, comp);
        if (child == NULL) {
            printf("cd: '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }

        if (child->type == SYMBOLIC) {
            Node *target = resolveSym(fs, child);
            if (target == NULL || target->type != DIRECTORY) {
                printf("cd: '%s': Not a directory\n", path);
                PathFree(p);
                return;
            }
            // canon stays at the symlink
            // real moves to the target
            canon = child;
            real = target;
        } else if (child->type == DIRECTORY) {
            // both canon and real move to this directory
            canon = child;
            real = child;
        } else {
            printf("cd: '%s': Not a directory\n", path);
            PathFree(p);
            return;
        }

        // append component to logical path str
        if (strlen(newPath) == 0 || strcmp(newPath, "/") == 0) {
            sprintf(newPath, "/%s", comp);
        } else {
            strcat(newPath, "/");
            strcat(newPath, comp);
        }
    }

    fs->cwd = canon;
    if (strlen(newPath) == 0) {
        strcpy(fs->cwdPath, "/");
    } else {
        strcpy(fs->cwdPath, newPath);
    }
    PathFree(p);
}

void FsLs(Fs fs, char *path) {
    Node *curr = NULL;

    // list current directory if no path given
    if (path == NULL) {
        curr = fs->cwd;
        for (Node *c = curr->children; c != NULL; c = c->next) {
            printf("%s\n", c->name);
        }
        return;
    }

    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);

    // traverse to target location
    curr = PathIsAbsolute(p) ? fs->root : fs->cwd;
    for (int i = 0; i < len; i++) {
        char *comp = PathComponent(p, i);
        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) curr = curr->parent;
            continue;
        }
        
        Node *child = findChild(curr, comp);
        if (child == NULL) {
            printf("ls: cannot access '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }
        // for intermediate path components, must be directory
        if (i < len - 1) {
            if (child->type == SYMBOLIC) {
                Node *target = resolveSym(fs, child);
                if (target == NULL || target->type != DIRECTORY) {
                    printf("ls: cannot access '%s': Not a directory\n", path);
                    PathFree(p);
                    return;
                }
                curr = target;
            } else if (child->type == DIRECTORY) {
                curr = child;
            } else {
                printf("ls: cannot access '%s': Not a directory\n", path);
                PathFree(p);
                return;
            }
        } else {
            curr = child;
        }
    }
    if (curr->type == SYMBOLIC) {
        Node *target = resolveSym(fs, curr);
        if (target == NULL) {
            printf("%s\n", path);
            PathFree(p);
            return;
        }
        if (target->type == DIRECTORY) {
            curr = target;
        }
    }

    if (curr->type != DIRECTORY) {
        printf("%s\n", path);
        PathFree(p);
        return;
    }

    // list directory contents
    for (Node *c = curr->children; c != NULL; c = c->next) {
        printf("%s\n", c->name);
    }

    PathFree(p);
}

void FsPwd(Fs fs) {
    if (fs == NULL) return;
    printf("%s\n", fs->cwdPath);
}

void FsTree(Fs fs, char *path, int limit, bool expand) {
    (void) limit;
    (void) expand;
    // path == NULL, starting from root
    if (path == NULL) {
        printf("/\n");
        for (Node *c = fs->root->children; c != NULL; c = c->next) {
            treePrint(c, 1);
        }
        return;
    }

    // path != NULL, follow the path
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);
    if (len == 0) {
        printf("/\n");
        for (Node *c = fs->root->children; c != NULL; c = c->next) {
            treePrint(c, 1);
        }
        PathFree(p);
        return;
    }

    Node *curr = PathIsAbsolute(p) ? fs->root : fs->cwd;

    for (int i = 0; i < len; i++) {
        char *comp = PathComponent(p, i);
        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) {
                curr = curr->parent;
            }
            continue;
        }

        Node *child = findChild(curr,comp);
        if (child == NULL) {
            printf("tree: '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }

        if (child->type == SYMBOLIC) {
            Node *target = resolveSym(fs, child);
            if (target == NULL) {
                printf("tree: '%s': No such file or directory\n", path);
                PathFree(p);
                return;
            }
            curr = target;
        } else {
            curr = child;
        }
    }

    if (curr->type != DIRECTORY) {
        printf("tree: '%s': Not a directory\n", path);
        PathFree(p);
        return;
    }

    printf("%s\n", path);
    for (Node *c = curr->children; c != NULL; c = c->next) {
        treePrint(c, 1);
    }

    PathFree(p);
}

void FsPut(Fs fs, char *path, char *content) {
    if (fs == NULL || path == NULL || content == NULL) return;
    
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);

    // navigate to target file
    Node *curr = PathIsAbsolute(p) ? fs->root : fs->cwd;
    for (int i = 0; i < len; i++) {
        char *comp = PathComponent(p, i);

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) curr = curr->parent;
            continue;
        }

        Node *child = findChild(curr, comp);
        if (child == NULL) {
            printf("put: '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }

        // for path components before the last, must traverse directory
        if (i < len - 1) {
            if (child->type == SYMBOLIC) {
                Node *target = resolveSym(fs, child);
                if (target == NULL) {
                    printf("put: '%s': No such file or directory\n", path);
                    PathFree(p);
                    return;
                }
                if (target->type != DIRECTORY) {
                    printf("put: '%s': Not a directory\n", path);
                    PathFree(p);
                    return;
                }
                curr = target;
            } else if (child->type == DIRECTORY) {
                curr = child;
            } else {
                printf("put: '%s': Not a directory\n", path);
                PathFree(p);
                return;
            }
        } else {
            curr = child;
        }
    }

    while (curr->type == SYMBOLIC) {
        Node *target = resolveSym(fs, curr);
        if (target == NULL) {
            printf("put: '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }
        curr = target;
    }    

    if (curr->type == DIRECTORY) {
        printf("put: '%s': Is a directory\n", path);
        PathFree(p);
        return;
    }
    // replace file content
    free(curr->content);
    curr->content = strdup(content);
    if (curr->content == NULL) {
        curr->content = NULL;
    }

    PathFree(p);
}

void FsCat(Fs fs, char *path) {
    Path p = PathParse(path);
    if (p == NULL) return;

    int len = PathLength(p);
    Node *curr = PathIsAbsolute(p) ? fs->root : fs->cwd;

    for (int i = 0; i < len; i++) {
        char *comp = PathComponent(p, i);

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) curr = curr->parent;
            continue;
        }

        Node *child = findChild(curr, comp);
        if (child == NULL) {
            printf("cat: '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }
        if (i < len - 1) {
            if (child->type == SYMBOLIC) {
                Node *target = resolveSym(fs, child);
                if (target == NULL) {
                    printf("cat: '%s': No such file or directory\n", path);
                    PathFree(p);
                    return;
                }
                if (target->type != DIRECTORY) {
                    printf("cat: '%s': Not a directory\n", path);
                    PathFree(p);
                    return;
                }
                curr = target;
            } else if (child->type == DIRECTORY) {
                curr = child;
            } else {
                printf("cat: '%s': Not a directory\n", path);
                PathFree(p);
                return;
            }
        } else {
            curr = child;
        }
   
    }

    while (curr->type == SYMBOLIC) {
        Node *target = resolveSym(fs, curr);
        if (target == NULL) {
            printf("cat: '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }
        curr = target;
    }

    if (curr->type == DIRECTORY) {
        printf("cat: '%s': Is a directory\n", path);
        PathFree(p);
        return;
    }

    if (curr->content != NULL) {
        printf("%s", curr->content);
    }

    PathFree(p);
}

void FsDldir(Fs fs, char *path) {
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);

    // navigate to parent directory of target
    Node *curr = PathIsAbsolute(p) ? fs->root : fs->cwd;
    for (int i = 0; i < len - 1; i++) {
        char *comp = PathComponent(p, i);

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) curr = curr->parent;
            continue;
        }

        Node *child = findChild(curr, comp);
        if (child == NULL) {
            printf("dldir: failed to remove '%s': No such file or directory\n", path);
            PathFree(p);
            return;
        }
        if (child->type == SYMBOLIC) {
            Node *target = resolveSym(fs, child);
            if (target == NULL) {
                printf("dldir: failed to remove '%s': No such file or directory\n", path);
                PathFree(p);
                return;
            }
            if (target->type != DIRECTORY) {
                printf("dldir: failed to remove '%s': Not a directory\n", path);
                PathFree(p);
                return;
            }
            curr = target;
        } else if (child->type != DIRECTORY) {
            printf("dldir: failed to remove '%s': Not a directory\n", path);
            PathFree(p);
            return;
        } else {
            curr = child;
        }
    }
    // get the directory name to delete
    char *name = PathComponent(p, len - 1);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("dldir: failed to remove '%s': Directory not empty\n", path);
        PathFree(p);
        return;
    }

    Node *target = findChild(curr, name);
    if (target == NULL) {
        printf("dldir: failed to remove '%s': No such file or directory\n", path);
        PathFree(p);
        return;
    }

    if (target->type != DIRECTORY) {
        printf("dldir: failed to remove '%s': Not a directory\n", path);
        PathFree(p);
        return;
    }
    // only delete if directory is empty
    if (target->children != NULL) {
        printf("dldir: failed to remove '%s': Directory not empty\n", path);
        PathFree(p);
        return;
    }
    // remove from parent's children list
    Node *prev = NULL;
    Node *iter = curr->children;
    while (iter != NULL && iter != target) {
        prev = iter;
        iter = iter->next;
    }

    if (prev == NULL) {
        curr->children = target->next;
    } else {
        prev->next = target->next;
    }

    free(target->content);
    free(target->symbolic);
    free(target->name);
    free(target);
    PathFree(p);
}

void FsDl(Fs fs, bool recursive, char *path) {
    Path p = PathParse(path);
    if (p == NULL) return;
    int len = PathLength(p);

    Node *curr = edgeTest(fs, p, len - 1, true, "dl: cannot remove", path);
    if (curr == NULL) {
        PathFree(p);
        return;
    }

    char *name = PathComponent(p, len - 1);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("dl: cannot remove '%s': Is a directory\n", path);
        PathFree(p);
        return;
    }

    Node *target = findChild(curr, name);
    if (target == NULL) {
        printf("dl: cannot remove '%s': No such file or directory\n", path);
        PathFree(p);
        return;
    }
    // only delete if recursive is set
    if (target->type == DIRECTORY && !recursive) {
        printf("dl: cannot remove '%s': Is a directory\n", path);
        PathFree(p);
        return;
    }
    // find and unlink from parent's children list
    Node *prev = NULL;
    Node *iter = curr->children;
    while (iter != NULL && iter != target) {
        prev = iter;
        iter = iter->next;
    }

    if (iter == NULL) {
        printf("dl: cannot remove '%s': No such file or directory\n", path);
        PathFree(p);
        return;
    }

    if (prev == NULL) {
        curr->children = target->next;
    } else {
        prev->next = target->next;
    }

    if (target->type == DIRECTORY) {
        freeNodeList(target->children);
        target->children = NULL;
    }
    free(target->content);
    free(target->symbolic);
    free(target->name);
    free(target);
    PathFree(p);
}

int FsMu(Fs fs, char *path, int limit, bool expand) {
    (void) fs;
    (void) path;
    (void) limit;
    (void) expand;
    return -1;
}


static void freeNodeList(Node *n) {
    while (n != NULL) {
        Node *next = n->next;
        freeNodeList(n->children);
        free(n->content);
        free(n->symbolic);
        free(n->name);
        free(n);
        n = next;
    }
}

static Node *newNode(char *name, FileType type, Node *parent) {
    Node *n = malloc(sizeof(*n));
    if (n == NULL) return NULL;
    n->name = strdup(name);
    if (n->name == NULL) {
        free(n);
        return NULL;
    }
    n->type = type;
    n->parent = parent;
    n->symbolic = NULL;
    n->children = NULL;
    n->next = NULL;
    n->content = NULL;
    return n;
}

static Node *findChild(Node *dir, const char *name) {
    for (Node *c = dir->children; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

static Node *edgeTest(
    Fs fs, Path p, int stop, bool follow_sym,
    const char *cmd_1, const char *path
) {
    Node *curr = PathIsAbsolute(p) ? fs->root : fs->cwd;

    for (int i = 0; i < stop; i++) {
        char *comp = PathComponent(p, i);

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) {
                curr = curr->parent;
            } else {
                curr = fs->root;
            }
            continue;
        }

        Node *child = findChild(curr, comp);
        if (child == NULL) {
            printf("%s '%s': No such file or directory\n",cmd_1, path);
            return NULL;
        }

        // handle symlinks based on follow_sym
        if (child->type == SYMBOLIC) {
            if (!follow_sym) {
                // stop at the symlink
                curr = child;
            } else {
                // follow the symlink to its target
                Node *target = resolveSym(fs, child);
                if (target == NULL) {
                    printf("%s '%s': No such file or directory\n", cmd_1, path);
                    return NULL;
                }
                if (target->type != DIRECTORY) {
                    printf("%s '%s': Not a directory\n", cmd_1, path);
                    return NULL;
                }
                curr = target;
            }
        } else if (child->type != DIRECTORY) {
            printf("%s '%s': Not a directory\n", cmd_1, path);
            return NULL;
        } else {
            curr = child;
        }
    }

    return curr;
}

static void insertChild(Node *parent, Node *child) {
    if (parent->children == NULL) {
        parent->children = child;
        child->next = NULL;
        return;
    }
    // find correct position to maintain alphabetical order
    Node *prev = NULL;
    Node *curr = parent->children;

    while (curr != NULL && strcmp(curr->name, child->name) < 0) {
        prev = curr;
        curr = curr->next;
    }

    if (prev == NULL) {
        child->next = parent->children;
        parent->children = child;
    } else {
        prev->next = child;
        child->next = curr;
    }
}

static void treePrint(Node *n, int depth) {
    if (n == NULL) return;
    // print indentation based on depth
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }
    // print node name 
    if (n->type == SYMBOLIC) {
        printf("%s -> %s\n", n->name, n->symbolic);
    } else {
        printf("%s\n", n->name);
    }
    if (n->type == DIRECTORY) {
        for (Node *c = n->children; c != NULL; c = c->next) {
            treePrint(c, depth + 1);
        }
    }
}

static Node *resolveSym(Fs fs, Node *link) {
    if (link->symbolic == NULL) return NULL;

    Path p = PathParse(link->symbolic);
    if (p == NULL) return NULL;
    
    // determine starting point
    Node *curr;
    if (link->symbolic[0] == '/') {
        curr = fs->root;
    } else {
        curr = link->parent;
    }

    int len = PathLength(p);
    for (int i = 0; i < len; i++) {
        char *comp = PathComponent(p, i);

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            if (curr->parent != NULL) curr = curr->parent;
            continue;
        }

        Node *child = findChild(curr, comp);
        if (child == NULL) {
            PathFree(p);
            return NULL;
        }

        if (child->type == SYMBOLIC) {
            Node *target = resolveSym(fs, child);
            if (target == NULL) {
                PathFree(p);
                return NULL;
            }
            curr = target;
        } else {
            curr = child;
        }
    } 

    PathFree(p);
    return curr;
}

static Node *canonical(Fs fs, Node *canon) {
    if (canon == fs->root) return fs->root;

    // build path from root to canon by collecting names
    char *names[PATH_MAX];
    int n = 0;

    Node *c = canon;
    // walk backwards
    while (c != NULL && c != fs->root) {
        names[n++] = c-> name;
        c = c->parent;
    }
    // traverse forward from root
    Node *real = fs->root;
    // process names in reverse order
    for (int i = n - 1; i >= 0; i--) {
        Node *child = findChild(real, names[i]);
        if (child == NULL) return NULL;
        // if hit a symlink, resolve and continue from target
        if (child->type == SYMBOLIC) {
            Node *target = resolveSym(fs, child);
            if (target == NULL) return NULL;
            real = target;
        } else {
            real = child;
        }
    }
    return real;
}