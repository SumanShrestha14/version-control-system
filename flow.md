# Synk — Execution Flow & Architecture Reference

This document explains **how a command travels through the program**, end to end, and gives an
**overview of every class and its major methods**. It reflects the current codebase (six-layer
OOP architecture: `core → storage → repository → workdir → net → cli`), not the original
procedural MVP.

---

## 1. High-Level Flow (every command, in general)

```
argv[]  →  main.cpp  →  Repository::open() (unless init/clone)
                     →  CommandParser::parse()  →  concrete *Command object
                     →  cmd->execute()  →  uses Repository / ObjectStore / RefStore / etc.
                     →  exit code
```

Concretely, in `main.cpp`:

1. `curl_global_init()` — libcurl needs one-time global init (used by `clone`).
2. Decide whether the requested command **needs an existing repo**. `init` and `clone` are the
   only two exceptions (they create a repo rather than require one).
3. If a repo is needed: `Repository::open(".")` is called. This looks for a `.synk` directory in
   the current folder. If it's missing, an exception is thrown, printed, and the program exits
   with failure — **before any command object is even constructed**.
4. `CommandParser::parse(argc, argv, repo.get())` inspects `argv[1]` (the sub-command name),
   validates its specific argument shape, and constructs **one concrete `Command` subclass**
   wired up with a reference to the repository (if any) and its parsed arguments.
5. `cmd->execute()` is called polymorphically through the abstract `Command` base — this is the
   **Command design pattern**: `main.cpp` never knows or cares which concrete command it's
   running.
6. The return code from `execute()` becomes the process exit code.

This is why the `Command` hierarchy exists at all: it turns "a pile of `if (command ==
"...")` branches with inline logic" into "one small, independently testable class per
sub-command," which is both easier to grade (one class per file) and easier to extend (adding a
command = adding a new `Command` subclass + one `if` in the parser).

---

## 2. Per-Command Flow

### 2.1 `synk init`

```
main.cpp
 └─ command == "init"  →  no repo required, repo pointer stays null
     └─ CommandParser::parse()
         └─ new InitCommand(root=".")
             └─ InitCommand::execute()
                 └─ Repository::init(root)
                     ├─ throws if root/.synk already exists
                     ├─ creates .synk/objects/
                     ├─ creates .synk/refs/heads/
                     └─ RefStore::setHeadToBranch("main")
                         └─ writes ".synk/HEAD" containing "ref: refs/heads/main\n"
                 └─ prints "Initialized synk directory"
```

`init` is special-cased in `main.cpp` because, by definition, no repository exists yet to open.

### 2.2 `synk hash-object [-w] <file>`

```
CommandParser::parse()
 └─ new HashObjectCommand(repo, filePath, write)
     └─ HashObjectCommand::execute()
         ├─ reads the file into a std::string (binary mode)
         ├─ constructs Blob(content)                      [core layer]
         └─ if -w:  repo.objects().write(blob)             [storage layer]
                    ├─ ObjectId id = blob.hash()            → GitObject::hash()
                    │    = ObjectId::hashOf(blob.storeBytes())
                    │    storeBytes() = header() + serialize()
                    │    header()     = "blob <size>\0"
                    ├─ if id already exists on disk → return id unchanged (content-addressed
                    │    dedup — Git never stores the same content twice)
                    ├─ else: Compressor::compress(storeBytes) via zlib
                    └─ write compressed bytes to .synk/objects/xx/yyyy...
            else:    blob.hash()   (compute only, don't write)
         └─ prints the resulting hex SHA-1
```

### 2.3 `synk cat-file -p <sha1>`

```
CommandParser::parse()
 └─ new CatFileCommand(repo, flag="-p", sha1)
     └─ CatFileCommand::execute()
         ├─ ObjectId::fromHex(sha1)                        [core layer — validates/stores hex]
         ├─ repo.objects().readRawWithHeader(id)            [storage layer]
         │    ├─ builds path .synk/objects/xx/yyyy...
         │    ├─ reads raw compressed bytes
         │    └─ Compressor::decompress()  → "<type> <size>\0<body>"
         └─ finds the '\0', prints everything after it (the raw object body)
```

Note `cat-file` deliberately does **not** go through `ObjectStore::read()` (which parses into a
`GitObject` subclass) — it needs the *exact* raw bytes as stored, which is why
`readRawWithHeader()` exists as a separate, more primitive method.

### 2.4 `synk write-tree`

```
CommandParser::parse()
 └─ new WriteTreeCommand(repo)
     └─ WriteTreeCommand::execute()
         └─ TreeBuilder(repo.objects()).writeTreeFromDirectory(repo.root())  [workdir layer]
             └─ buildSubtree(dir)                     (recursive, bottom-up)
                 for each directory entry:
                   ├─ skip ".synk" itself
                   ├─ symlink?  → skip with a warning (mode 120000 not implemented)
                   ├─ is directory? → recurse: buildSubtree(subdir)
                   │     empty result → skip (Git doesn't track empty dirs)
                   │     else → TreeEntry("40000", name, subtreeId)
                   └─ is regular file? → read bytes → Blob → store_.write(blob)
                         mode = "100755" if executable else "100644"
                         → TreeEntry(mode, name, blobId)
                 ├─ construct Tree(entries), sortEntries()  (Git's dir-as-if-"/" sort rule)
                 └─ store_.write(tree)                      [storage layer, same content-
                                                              addressed write path as blobs]
         └─ prints the resulting tree's hex SHA-1
```

### 2.5 `synk commit-tree <tree_sha> [-p <parent_sha>] -m <message>`

```
CommandParser::parse()
 └─ new CommitTreeCommand(repo, treeSha, parentSha, message)
     └─ CommitTreeCommand::execute()
         ├─ ObjectId::fromHex(treeSha)
         ├─ parents = { ObjectId::fromHex(parentSha) } if given, else empty
         ├─ authorLine = "Suman <suman@example.com> " + GitTimestamp::now()
         │    GitTimestamp::now() → "<epoch_seconds> <±HHMM>"  (Git's timestamp format)
         ├─ Commit commit(treeId, parents, authorLine, authorLine, message)
         └─ repo.objects().write(commit)   [same content-addressed write path]
             Commit::serialize() builds:
               "tree <hex>\n"
               "parent <hex>\n"   (zero or more)
               "author <line>\n"
               "committer <line>\n"
               "\n<message>"
         └─ prints the resulting commit's hex SHA-1
```

### 2.6 `synk ls-tree [--name-only] <tree_sha>`

```
CommandParser::parse()
 └─ new LsTreeCommand(repo, sha1, nameOnly)
     └─ LsTreeCommand::execute()
         ├─ repo.objects().read(id)                         [storage layer]
         │    ├─ readRawWithHeader(id) → decompressed "<type> <size>\0<body>"
         │    └─ GitObject::parse(storeBytes)                [core layer — factory]
         │         dispatches on the type word to Tree::parse / Blob::parse / Commit::parse
         ├─ dynamic_cast<Tree*>(obj.get())  — fails → "fatal: ... is not a tree object"
         └─ for each TreeEntry in tree->entries():
               --name-only → print entry.name()
               else        → print "<paddedMode> <typeName> <sha>\t<name>"
```

### 2.7 `synk clone <url> <dir>`

This is the most involved flow — it drives the `net` layer, which in turn writes into the same
`storage`/`repository`/`workdir` layers used by every other command.

```
CommandParser::parse()
 └─ new CloneCommand(url, targetDir)     — no repo required yet; clone creates its own
     └─ CloneCommand::execute()
         └─ CloneOperation(url, targetDir).run()                       [net layer]
             │
             ├─ (1) discoverRefs()
             │     GET  <url>/info/refs?service=git-upload-pack
             │     PktLineCodec::parse(response)                       [net layer, pkt-line]
             │     → collects (sha, refName) pairs, HEAD sha, and the
             │       "symref=HEAD:refs/heads/<branch>" capability (default branch)
             │
             ├─ empty repo? (headSha is all zeros) → just create targetDir, return early
             │
             ├─ (2) fetchPackfile(headSha)
             │     POST <url>/git-upload-pack
             │       body = pkt-line("want <sha> side-band-64k agent=synk/0.1")
             │            + pkt-line("")            (flush)
             │            + pkt-line("done")
             │
             ├─ (3) extractPackData(rawResponse)
             │     PktLineCodec::parse() again, then demux the side-band-64k channels:
             │       channel 1 = packfile data   → appended to packfileData
             │       channel 2 = progress text   → printed to stderr
             │       channel 3 = fatal error     → thrown as an exception
             │     (falls back to scanning for a raw "PACK" signature if no side-band framing
             │      is detected — protects against servers that ignore the capability)
             │
             ├─ writes the raw pack to <targetDir>/downloaded.pack (debugging aid)
             │
             ├─ Repository::init(targetDir)     — same init path as `synk init`, but rooted
             │                                     at the clone destination
             │
             ├─ (4) PackfileParser(packfileData).parse(headerCount)   [net layer]
             │     walks the 12-byte pack header, then for each of headerCount entries:
             │       readTypeAndSize()      — packfile's own varint-with-type encoding
             │       if OfsDelta  → readOfsDeltaOffset()   ("+1 per continuation byte" quirk)
             │       if RefDelta  → read 20 raw base-object bytes
             │       inflateNextStream()    — zlib-inflate this entry's compressed body
             │     → vector<PackObject>  (deltas NOT yet applied)
             │
             ├─ (5) DeltaResolver::resolve(packObjects)               [net layer]
             │     resolves OfsDelta/RefDelta chains of arbitrary depth against their bases
             │     (which may themselves be deltas) via applyDelta(), producing fully
             │     reconstructed object bytes for every entry
             │     → vector<ResolvedObject>  { type, fully-reconstructed body }
             │
             ├─ for each ResolvedObject: repo.objects().writeRaw(typeName, data)
             │     [storage layer — same content-addressed disk layout as write()/writeRaw()]
             │
             ├─ (6) repo.refs().writeRef("refs/heads/<branch>", headId)
             │     repo.refs().setHeadToBranch(branch)                [repository layer]
             │
             └─ (7) Checkout(repo.objects()).checkoutCommit(headId, targetDir)  [workdir layer]
                   ├─ reads the commit object, dynamic_cast to Commit*
                   └─ checkoutTree(commit->tree(), targetDir)          (recursive)
                         for each TreeEntry:
                           directory  → mkdir, recurse
                           submodule  → skip with a warning (gitlink not implemented)
                           file       → read Blob, write bytes to disk,
                                        chmod +x if entry.isExecutable()
                           symlink    → (not reached; TreeBuilder never wrote one)
```

---

## 3. Class Overview by Layer

The architecture is **bottom-up**: each layer only depends on layers below it.
`core` has no project dependencies; `cli` depends on everything.

```
cli          (command objects + argv parsing)
  ↓
net          (HTTP transport, pkt-line, packfile parsing, delta resolution — clone only)
workdir      (working-directory ⇄ object translation: building trees, checking trees out)
repository   (ties ObjectStore + RefStore together as one repo handle; ref/HEAD management)
storage      (on-disk object read/write, compression)
core         (in-memory Git object model: blob/tree/commit, hashing, IDs, timestamps)
```

### 3.1 `core` — the object model

| Class | Purpose | Key methods |
|---|---|---|
| **`ObjectId`** | Represents a SHA-1 identity. Always stores **hex** internally so call sites never have to guess whether a string is hex or raw bytes (this ambiguity was the root cause of an earlier "double-hashing" bug class). | `fromHex(hex)`, `fromRaw(raw20)`, `hashOf(storeBytes)` — static factories; `hex()`, `raw()` — accessors (raw converts on demand). |
| **`GitObject`** (abstract) | Base class for the three object kinds Git stores. Owns the shared framing logic so subclasses only implement their own body encoding. | `type()` (pure virtual), `serialize()` (pure virtual, body only) — `typeName()`, `header()` ("blob 12\0"), `storeBytes()` (`header()+serialize()`), `hash()` (`ObjectId::hashOf(storeBytes())`); `static parse(storeBytes)` — factory that dispatches on the type word to the right subclass's `parse()`. |
| **`Blob`** | A file's raw content, unmodified. | `content()`; `static parse(storeBytes)`. |
| **`Tree`** | An ordered set of `TreeEntry` (one directory snapshot). | `entries()`, `addEntry()`, `sortEntries()` (Git's byte-exact sort order); `serialize()` encodes entries in sorted order; `static parse(storeBytes)`. |
| **`TreeEntry`** | One row of a tree: mode + name + child SHA. | `isDirectory()`, `isSubmodule()`, `isExecutable()`; `typeName()`, `paddedMode()` (for `ls-tree -l`); `sortKey()` — appends a trailing `/` for directories, matching real Git's sort behavior; `encode()` — `"<mode> <name>\0<20 raw bytes>"`. |
| **`Commit`** | Tree pointer + parent(s) + author/committer + message. | `tree()`, `parents()`, `message()`; `serialize()` builds the `tree/parent/author/committer/\n<message>` text; `static parse(storeBytes)`. |
| **`GitTimestamp`** | Stateless helper for Git's timestamp format. | `static now()` → `"<epoch_seconds> <±HHMM>"`. |

### 3.2 `storage` — on-disk persistence

| Class | Purpose | Key methods |
|---|---|---|
| **`Compressor`** | Stateless zlib wrapper. | `static compress(data)`, `static decompress(compressedData)`. |
| **`ObjectStore`** | Owns the `.synk/objects/xx/yyyy...` layout and all disk I/O for objects. Content-addressed: writing the same content twice is a no-op the second time. | `write(GitObject&)` → hash, skip-if-exists, compress, write, return `ObjectId`; `writeRaw(type, body)` — same path but for pre-typed bytes arriving from `clone`'s pack entries rather than a `GitObject` instance; `read(id)` → decompress + `GitObject::parse()` (typed result); `readRawWithHeader(id)` → decompress only, no parse (used by `cat-file`, which must reproduce exact raw bytes); `exists(id)`. |

### 3.3 `repository` — repo-level state

| Class | Purpose | Key methods |
|---|---|---|
| **`RefStore`** | Reads/writes refs and HEAD under a given `.synk` directory. | `writeRef(name, id)`, `readRef(name)`, `refExists(name)`; `setHeadToBranch(branch)` — writes `"ref: refs/heads/<branch>\n"` to HEAD; `resolveHead()` — HEAD → its target ref → that ref's `ObjectId` (⚠️ **throws** on detached HEAD or no commits yet — flagged as the current blocker for `log`/`status`); `currentBranch()` — strips `refs/heads/` from HEAD's contents. |
| **`Repository`** | The main handle a command works against: bundles one `ObjectStore` + one `RefStore` rooted at the same `.synk` directory. | `static init(root)` — creates `.synk/objects`, `.synk/refs/heads`, sets HEAD to `main`, throws if `.synk` already exists; `static open(root)` — throws if `.synk` is missing (no upward directory search yet); `objects()`, `refs()`, `root()`, `synkDir()` — accessors used by every command. |

### 3.4 `workdir` — translating between the object store and real files

| Class | Purpose | Key methods |
|---|---|---|
| **`TreeBuilder`** | Turns a real directory into a `Tree` object graph (`write-tree`'s engine). | `writeTreeFromDirectory(dir)` — public entry point, handles the "root is totally empty" edge case by still writing an empty-tree object; `buildSubtree(dir)` (private, recursive) — walks entries bottom-up, skips `.synk` and symlinks (with a warning), recurses into subdirectories, writes a `Blob` per file (mode `100644`/`100755`), assembles + sorts + writes a `Tree`, returns `std::nullopt` if the directory contributed nothing so empty dirs aren't tracked. |
| **`Checkout`** | The inverse of `TreeBuilder`: materializes a tree/commit's contents onto disk. Used by `clone`. | `checkoutCommit(commitId, dest)` — resolves the commit's tree, delegates to `checkoutTree`; `checkoutTree(treeId, dest)` (recursive) — creates directories, recurses into subtrees, writes blob content to files, sets the executable bit when `TreeEntry::isExecutable()`, warns and skips submodules (gitlink not implemented). |

### 3.5 `net` — transport and packfile protocol (clone only)

| Class | Purpose | Key methods |
|---|---|---|
| **`HttpClient`** | Thin RAII wrapper around a single libcurl easy handle; non-copyable (owns a resource). | `get(url, headers)`, `post(url, body, headers)` → `HttpResponse{statusCode, body}`. |
| **`PktLineCodec`** | Stateless codec for Git's pkt-line wire framing. | `static parse(data)` → `vector<PktLine>` (each with `isFlush` + `data`); `static format(payload)` — wraps a payload in the 4-hex-digit length prefix. |
| **`PackfileParser`** | Parses a raw `.pack` byte stream into individual (still-possibly-delta) entries. | `parse(headerObjectCount&)` — walks the 12-byte header, then per-entry: `readTypeAndSize()` (packfile's type+size varint), `readOfsDeltaOffset()` (ofs-delta's "+1 per continuation byte" varint quirk), `inflateNextStream()` (zlib-inflate exactly this entry's compressed span) → `vector<PackObject>`. |
| **`DeltaResolver`** | Stateless; resolves ofs-delta/ref-delta chains of arbitrary depth into final object bytes. | `static applyDelta(base, delta)` — applies one delta's copy/insert instructions against a base; `static resolve(objects)` — resolves every `PackObject` in a pack (walking chains as needed) → `vector<ResolvedObject>`. |
| **`CloneOperation`** | Orchestrates the whole clone protocol exchange; the only class that talks to all of `net`, `repository`, `storage`, and `workdir` in one flow. | `run()` — see §2.7 for the full sequence; `discoverRefs()` (private) — GET `/info/refs`, parse ref advertisement + `symref=HEAD` capability; `fetchPackfile(wantSha)` (private) — POST `/git-upload-pack` with a `want`/`done` negotiation; `extractPackData(rawResponse)` (private) — demux side-band-64k channels, with a raw-`PACK`-signature fallback. |

### 3.6 `cli` — command objects and argv parsing

| Class | Purpose | Key methods |
|---|---|---|
| **`Command`** (abstract) | The Command-pattern base every sub-command implements. | `execute()` (pure virtual) → process exit code. |
| **`CommandParser`** | The only class that touches `argv` directly. Validates each command's specific argument shape and constructs the matching `Command` subclass. | `static parse(argc, argv, repo*)` — one `if (command == "...")` branch per sub-command; calls a local `requireRepo()` helper that throws if a repo-dependent command was invoked without an open repository. |
| **`InitCommand`** | `synk init`. | `execute()` → `Repository::init(root)`. |
| **`HashObjectCommand`** | `synk hash-object [-w] <file>`. | `execute()` → read file → `Blob` → `write()` if `-w`, else `hash()` only. |
| **`CatFileCommand`** | `synk cat-file -p <sha1>`. | `execute()` → `readRawWithHeader()`, print body after the `\0`. |
| **`LsTreeCommand`** | `synk ls-tree [--name-only] <sha1>`. | `execute()` → `ObjectStore::read()`, `dynamic_cast<Tree*>`, print entries. |
| **`WriteTreeCommand`** | `synk write-tree`. | `execute()` → `TreeBuilder(repo.objects()).writeTreeFromDirectory(repo.root())`. |
| **`CommitTreeCommand`** | `synk commit-tree <tree> [-p <parent>] -m <msg>`. | `execute()` → build `Commit`, `write()` it. |
| **`CloneCommand`** | `synk clone <url> <dir>`. | `execute()` → `CloneOperation(url, dir).run()`. |

---

## 4. Known Gaps Reflected in This Flow

- `RefStore::resolveHead()` / `currentBranch()` **throw** on a detached HEAD or a freshly-`init`'d
  repo with no commits yet. This must become non-throwing (e.g. return `std::optional<ObjectId>`)
  before `log` or `status` can be built on top of it.
- `TreeBuilder::buildSubtree()` and `Checkout::checkoutTree()` both skip symlinks/submodules with
  a warning rather than materializing them — mode `120000` and gitlinks are not yet implemented.
- No `add`/porcelain `commit` yet — `write-tree` always snapshots the *entire* working directory,
  since there is no `.synk/index` (staging area) in this architecture yet.

---

## 5. Where to Add the Next Command

Following this flow, adding e.g. `synk log` means:

1. Add a non-throwing HEAD-resolution method to `RefStore` (the current blocker).
2. Create `LogCommand : public Command` in `cli/` (one file per class, matching the existing
   pattern) that walks `commit->parents()` from HEAD, printing each commit.
3. Add one `if (command == "log") { ... }` branch to `CommandParser::parse()`.
4. Register the new `.cpp`/`.h` pair in `CMakeLists.txt` and re-run `cmake -B build` (a reconfigure
   is required for new files — `cmake --build build` alone won't pick them up).
