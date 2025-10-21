# httplib

Small HTTP library, written in C from scratch.

> [!WARNING]
> This project was created **solely for learning purposes** — to explore how
> one might implement HTTP library from scratch in C.

> [!WARNING]
> This project is still WIP.

## Usage

#### TODO: Quick-start guide

There are 4 levels of logs: `TODO`, `INFO`, `WARN`, `ERROR`. To disable any of
them you should simply redefine macro `HTTP_<LEVEL>(...)` to nothing before
including `http.h`, e.g.:
```c
#define HTTP_TODO(...)
#define HTTP_INFO(...)
#define HTTP_WARN(...)
#define HTTP_ERROR(...)

#define HTTP_IMPL
#include "http.h"
```
