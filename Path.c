#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "Path.h"

struct path {
    char *path;
    char **components;
    int nComponents;
    bool isAbsolute;
};

static void addComponent(Path p, char *buffStart, int buffLen);

Path PathParse(char *pathStr) {
    Path p = calloc(1, sizeof (*p));
    p->path = strdup(pathStr);
    p->components = malloc(0);

    char *buffStart = pathStr;
    int buffLen = 0;
    char *currChar = pathStr - 1;

    while (*(++currChar) != '\0') {
        if (*currChar != '/') {
            buffLen++;
            continue;
        }
        if (buffLen == 0) {
            if (p->nComponents == 0)
                p->isAbsolute = true;
            buffStart++;
            continue;
        }

        addComponent(p, buffStart, buffLen);
        buffStart = currChar + 1;
        buffLen = 0;
    }

    if (buffLen > 0)
        addComponent(p, buffStart, buffLen);
    return p;
}

static void addComponent(Path p, char *buffStart, int buffLen) {
    char *component = malloc((buffLen + 1) * sizeof (char));
    strncpy(component, buffStart, buffLen)[buffLen] = '\0';
    p->components = realloc(p->components, (p->nComponents + 1) * sizeof (char *));
    p->components[p->nComponents++] = component;
}

void PathFree(Path p) {
    for (int i = 0; i < p->nComponents; i++)
        free(p->components[i]);
    free(p->components);
    if (p->path)
        free(p->path);
    free(p);
    return;
}

int PathLength(Path p) {
    return p->nComponents;
}

bool PathIsAbsolute(Path p) {
    return p->isAbsolute;
}

char *PathComponent(Path p, int n) {
    if (n < 0)
        n = PathLength(p) + n;
    if (n < 0 || n >= p->nComponents)
        return NULL;
    return p->components[n];
}

char *PathString(Path p) {
    return p->path;
}