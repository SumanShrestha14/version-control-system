# Synk 🔗

**Synk** is a from-scratch reimplementation of Git's core plumbing, written in modern C++23. It's built for two purposes at once: as a submission for the [CodeCrafters "Build Your Own Git" challenge](https://codecrafters.io/challenges/git), and as an Object-Oriented Programming course project at **Tribhuvan University, Institute of Engineering (IOE), Purwanchal Campus**.

Rather than treating Git as a black box, this project peels it apart — content-addressable storage, zlib-compressed objects, tree/commit serialization, the binary index format, and the Smart HTTP transport protocol — and rebuilds each piece by hand.

> **Note on naming:** the binary and internal command set are called `synk` / `.synk` throughout the codebase, to keep this implementation clearly distinguished from the real `git` / `.git` on disk.

---

## Table of Contents

- [Why "Synk"?](#why-synk)
- [Features](#features)
- [Project Status](#project-status)
- [Git Internals — A Quick Primer](#git-internals--a-quick-primer)
- [Architecture](#architecture)
- [Getting Started](#getting-started)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Design Decisions & Trade-offs](#design-decisions--trade-offs)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)
- [Team](#team)
- [Acknowledgements & References](#acknowledgements--references)

---

## Why "Synk"?

The project began as a direct `git` clone (`git.exe`, `.git/`, `git_init()`, etc.), but was renamed mid-development to **Synk** — a **syn**thetic Git — to avoid ambiguity between "the real git binary on your PATH" and "our binary." Every internal identifier was renamed to match: `synk_init`, `synk_hash_object`, `.synk/`, and so on. The object model, algorithms, and on-disk formats remain byte-compatible with real Git wherever practical, since correctness is verified by diffing against actual `git` output.

## Features

Synk currently implements the following Git plumbing commands, verified against real `git` behavior:

| Command | Description |
|---|---|
| `synk init` | Initializes a new repository (`.synk/` directory structure) |
| `synk cat-file` | Reads and prints the content of a stored object, given its hash |
| `synk hash-object` | Hashes a file's content and optionally writes it as a blob object |
| `synk ls-tree` | Lists the contents of a tree object |
| `synk write-tree` | Recursively writes the working directory as a tree object |
| `synk commit-tree` | Creates a commit object pointing at a given tree |
| `synk clone` | Clones a remote repository over the Smart HTTP protocol |



## Project Status

**Actively in development.** See the [Roadmap](#roadmap) for what's next, and [Known Limitations](#known-limitations) for explicit, honest gaps rather than silent ones.

Verified end-to-end so far:
- Object hashing/storage round-trips exactly match real Git's SHA-1 hashes for blobs, trees, and commits.
- The full `clone` pipeline (ref discovery → `upload-pack` negotiation → packfile download → object parsing, including delta objects → ref/HEAD writing → working directory checkout) has been tested against a real GitHub repository.
- Index staging has been implemented; cross-verification against `git status` / `git ls-files --stage` is in progress.

## Git Internals — A Quick Primer

Before touching the code, it's worth understanding *what* we're actually reimplementing. This section is deliberately explanatory — one of this project's goals is to understand Git deeply, not just to produce a working binary.

### 1. The Object Database — content-addressable storage

Everything Git tracks is stored as an **object**, identified by the SHA-1 hash of its own content. There are three object types relevant to plumbing:

- **Blob** — the raw contents of a single file (no filename, no permissions — just bytes).
- **Tree** — a directory listing: a sequence of `(mode, name, SHA-1)` entries, each pointing at a blob (file) or another tree (subdirectory).
- **Commit** — a snapshot: a pointer to one root tree, zero or more parent commits, author/committer metadata, and a message.

Every object is stored as:

```
"<type> <content-length>\0<content>"   →   zlib-deflate   →   write to .synk/objects/<first 2 hex chars>/<remaining 38 hex chars>
```

The SHA-1 hash is computed **over the uncompressed header + content**, before compression — this is why "double-hashing" (accidentally hashing already-hashed bytes, or hashing the compressed form) is such an easy and common bug.

Tree entries store the SHA-1 as **raw 20 bytes**, not as a 40-character hex string — a detail that trips up a lot of from-scratch implementations, including ours initially.

### 2. Commits and the DAG

A commit is just a plain-text object referencing a tree SHA and (optionally) parent commit SHA(s). Because each commit points backward to its parent(s), the full commit history forms a **Directed Acyclic Graph (DAG)** — this is the entire mathematical basis for branching, merging, and history traversal in Git. There's no "branch" object; a branch is just a mutable pointer (a file under `.synk/refs/heads/`) to a commit SHA.


### 3. The Smart HTTP Transport (used by `clone`)

Cloning over HTTP is a negotiation protocol, not a simple file download:

1. **Reference discovery** — `GET .../info/refs?service=git-upload-pack` returns every ref the server has and its current SHA, encoded as **pkt-lines** (a simple length-prefixed line framing format).
2. **Negotiation** — the client sends a `POST .../git-upload-pack` request listing which refs it `want`s (and, for incremental fetches, which commits it already `have`s). Capabilities like `side-band-64k` must be declared in this request, or the server responds with a raw, undemarcated packfile stream instead of the expected multiplexed one.
3. **Packfile transfer** — the server responds with a **packfile**: a compressed, delta-encoded archive of every object needed to satisfy the `want`s. Objects can be stored whole ("non-delta") or as a diff against another object in the pack ("delta"), referenced either by SHA (`ref-delta`) or by relative byte offset (`ofs-delta`). Packfiles use three *different* variable-length integer encodings across their header, delta-offset, and delta-size fields — a subtlety worth studying carefully rather than assuming one varint format fits all.
4. **Unpacking & checkout** — the client parses every object out of the packfile (resolving deltas against their base objects), writes them into the object database, updates local refs/HEAD, and finally materializes the tree into the working directory.

### 4. Why zlib?

Every object Git stores on disk is zlib-deflate compressed. This is orthogonal to hashing (hashing happens on the *uncompressed* content) but essential for reading and writing objects that interoperate with real Git tooling.

---

## Architecture

### Design Philosophy: MVP First, OOP Second

We deliberately chose to build the **first working version procedurally** — plain functions operating on small structs, minimal abstraction — before introducing class hierarchies. The reasoning:

1. Git's binary formats and protocols are unforgiving; a bug (like raw-pointer string comparison, or a swapped varint decode) is much easier to isolate in flat, linear code than through several layers of inheritance.
2. Premature OOP abstraction risks modeling the *wrong* boundaries before we've actually seen where the real seams in the problem are.
3. Once each stage (hashing, tree serialization, packfile parsing, transport) is independently correct and tested, refactoring into cohesive classes is a mechanical, low-risk transformation — the opposite is not true.

The target OOP architecture (in progress) organizes around these responsibilities:

- **`ObjectStore`** — reading/writing/compressing objects in `.synk/objects/`; owns hashing and zlib logic.
- **`Blob` / `Tree` / `Commit`** — value types representing parsed object content, with `serialize()` / `deserialize()` methods.
- **`Index`** — parses/writes the binary `.synk/index`, exposes staging operations.
- **`Repository`** — top-level façade tying together the object store, index, and refs for a given `.synk/` root; entry point for command handlers.
- **`PackfileParser`** — decodes non-delta and delta object entries out of a downloaded packfile, resolving `ofs-delta`/`ref-delta` chains.
- **`HttpTransport`** — wraps libcurl for the Smart HTTP discovery + negotiation requests.
- **`Command` hierarchy** (`InitCommand`, `HashObjectCommand`, `CloneCommand`, ...) — one class per CLI subcommand, dispatched from `main()`.

## Getting Started

### Prerequisites

- A C++23-capable compiler (developed against **MinGW GCC via MSYS2**)
- [CMake](https://cmake.org/) + [Ninja](https://ninja-build.org/)
- [OpenSSL](https://www.openssl.org/) (SHA-1 hashing)
- [zlib](https://zlib.net/) (object compression)
- [libcurl](https://curl.se/libcurl/) (HTTP transport — chosen over raw sockets so effort goes into the Git protocol itself, not a socket/TLS layer)

On Windows, all of the above are available through MSYS2's MinGW64 package set. Make sure `C:\msys64\mingw64\bin` is on your `PATH` so the required DLLs (OpenSSL, zlib, libcurl, and the MinGW runtime) can be resolved at runtime — missing this step manifests as either a silent hang or a `0xC0000139` startup error, which looks identical to a logic bug if you don't know to check PATH first.

### Build

```bash
cmake -B build -G Ninja
cmake --build build
```

> **Note:** re-run `cmake -B build` (not just `cmake --build build`) whenever new `.cpp` files are added — CMake's file globbing is cached and won't pick up new sources otherwise, which shows up as a confusing linker "undefined reference" error.

The resulting binary is `build/synk.exe` (Windows) or `build/synk` (Linux/macOS).

### Run the tests

Never run the binary against this project's own `.synk`/`.git` directory. Always test inside a disposable sandbox:

```bash
mkdir -p build/test && cd build/test
../synk init
```

Delete and recreate `build/test` between test runs for a clean slate.

## Usage

```bash
synk init                                  # Initialize a new repository
synk cat-file -p <sha>                     # Pretty-print an object's content
synk hash-object -w <file>                 # Hash a file and store it as a blob
synk ls-tree --name-only <tree-sha>        # List a tree's contents
synk write-tree                            # Write the working directory as a tree object
synk commit-tree <tree-sha> -m "<message>" # Create a commit object
synk clone <url> <directory>               # Clone a remote repository
```

## Project Structure

```
.
├── src/                # All implementation source (commands, object model, transport)
├── CMakeLists.txt      # Build configuration
├── codecrafters.yml    # CodeCrafters challenge configuration
├── vcpkg.json          # Dependency manifest (if using vcpkg for OpenSSL/zlib/curl)
└── README.md
```

## Design Decisions & Trade-offs

| Decision | Rationale |
|---|---|
| libcurl over raw sockets | Keeps focus on the Git *protocol* (pkt-lines, negotiation, packfiles) instead of reimplementing HTTP/TLS from scratch. |
| Procedural MVP before OOP refactor | Easier to debug binary-format bugs in flat code; class boundaries are chosen *after* the real structure of the problem is understood, not guessed upfront. |
| Byte-compatible object format | Lets us verify correctness by diffing against real `git` output directly, rather than trusting our own tests in isolation. |
| Windows-specific field zeroing (`dev`, `ino`, `uid`, `gid`) in the index | These POSIX-only fields have no meaningful Windows equivalent; zeroing them is documented explicitly rather than left as an unexplained gap. |

## Known Limitations

We'd rather state gaps explicitly than paper over them:

- **Symlinks and submodules** are currently detected and skipped with a warning, not implemented.
- **`synk init` hang**: under investigation — currently isolated to a runtime dispatch issue rather than a `PATH`/DLL problem (those were previously ruled out and fixed separately). Likely candidates being audited: raw-pointer `argv` string comparisons (`argv[1] == "init"` compares pointer addresses, not string content — must convert to `std::string` first) and pre-`main()` blocking behavior.
- **Static linking / DLL bundling** for a self-contained CodeCrafters submission is not yet done; the binary currently depends on MSYS2 DLLs being present on `PATH`.
- **Index verification** against `git status` / `git ls-files --stage` is in progress, not yet fully cross-checked.

## Roadmap

- [ ] Resolve the `synk init` hang (dispatch-path diagnostics in progress)
- [ ] Cross-verify the index/staging implementation against real `git status` output
- [ ] Static linking or DLL bundling for a portable, dependency-free submission binary
- [ ] Refactor the verified procedural MVP into the target OOP class architecture
- [ ] Symlink and submodule support

## Team

Built by students at **Tribhuvan University, IOE, Purwanchal Campus**, as an OOP course project:

- Suman Shrestha
- Shourab Ghimire
- Saurav Adhikari
- Satyadev Mourya

*Subject Teacher: Mr. Bikram Shah*
