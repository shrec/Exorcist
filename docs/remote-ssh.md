# Remote / SSH Subsystem

The Remote subsystem provides SSH connectivity, remote file browsing, remote
terminal sessions, remote build/run, file synchronisation, and multi-architecture
host probing.  All SSH operations use the system `ssh`/`sftp`/`scp` executables
via QProcess — no native SSH library dependency.

## Directory: `src/remote/`

## Data types

| Type | File | Description |
|------|------|-------------|
| `SshProfile` | `sshprofile.h` | Connection profile (host, port, user, auth, key, work dir, env, detected arch/OS/distro). JSON serialisable. |
| `CpuArch` | `remotehostinfo.h` | Enum: `X86_64`, `AArch64`, `ARM32`, `RiscV64`, `RiscV32`, `X86`, `MIPS64`, `PPC64`, `S390X`, `LoongArch64`, `Unknown` |
| `RemoteHostInfo` | `remotehostinfo.h` | Probed host data: arch, OS, kernel, distro, cores, RAM, compilers, build systems, debuggers, package managers, target triple |

## Key classes

### SshSession (`sshsession.h/.cpp`)

QProcess-based async SSH/SFTP/SCP wrapper.

| Method | Description |
|--------|-------------|
| `connectToHost()` | Connectivity test via `ssh echo __EXORCIST_OK__` |
| `runCommand(cmd, workDir)` | Async command execution, returns request ID |
| `listDirectory(path)` | `ssh ls -1aF` for lazy tree loading |
| `readFile(path)` | `ssh cat` for file content |
| `writeFile(path, content)` | `ssh cat >` with stdin piping |
| `downloadFile(remote, local)` | `scp` download |
| `uploadFile(local, remote)` | `scp` upload |
| `exists(path)` | `ssh test -e` |

SSH base args: `BatchMode=yes`, `StrictHostKeyChecking=accept-new`, `ConnectTimeout=10`.

### SshConnectionManager (`sshconnectionmanager.h/.cpp`)

Profile CRUD + session lifecycle.

- Profiles persisted to `.exorcist/ssh_profiles.json`
- `connect(profileId)` → creates `SshSession`, returns pointer
- `activeSession(profileId)` → existing session or nullptr
- `disconnect(profileId)` / `disconnectAll()`
- Signals: `profilesChanged`, `connected`, `disconnected`, `connectionError`

### RemoteFileSystemModel (`remotefilesystemmodel.h/.cpp`)

`QAbstractItemModel` for remote file trees with lazy loading.

- `canFetchMore()`/`fetchMore()` triggers `SshSession::listDirectory()`
- Parses `ls -1aF` markers: `/` (dir), `*` (executable), `@` (symlink)
- `filePath(index)`, `isDir(index)` accessors
- Standard Qt icons for files/folders

### RemoteFilePanel (`remotefilepanel.h/.cpp`)

UI panel for SSH profile management and remote file browsing.

- Profile combo with add/edit/remove buttons
- Connect/Disconnect with state tracking
- Remote terminal button (launches SSH in TerminalPanel)
- Architecture info label (auto-populated by RemoteHostProber)
- QTreeView with RemoteFileSystemModel
- Signals: `openRemoteFile(remotePath, profileId)`, `openRemoteTerminal(profileId)`

### RemoteHostProber (`remotehostinfo.h/.cpp`)

Probes remote host via 4 parallel SSH commands:

1. `uname -m && uname -s && uname -r` → arch, OS, kernel
2. `cat /etc/os-release` → distro name + version
3. `nproc` + `/proc/meminfo` → CPU cores + RAM
4. `command -v` loop → available compilers, build systems, package managers, debuggers

Supported architectures:

| Arch | `uname -m` values |
|------|-------------------|
| x86_64 | `x86_64`, `amd64` |
| AArch64 (ARM64) | `aarch64`, `arm64` |
| ARM32 | `armv7l`, `armhf`, `armv*` |
| RISC-V 64 | `riscv64` |
| RISC-V 32 | `riscv32` |
| x86 (32-bit) | `i386`, `i486`, `i586`, `i686` |
| MIPS64 | `mips64`, `mips64el` |
| PowerPC 64 | `ppc64`, `ppc64le` |
| s390x | `s390x` |
| LoongArch64 | `loongarch64` |

`targetTriple()` generates cross-compilation triples: e.g. `aarch64-linux-gnu`, `riscv64-linux-gnu`, `x86_64-apple-darwin`.

### RemoteSyncService (`remotesyncservice.h/.cpp`)

Rsync wrapper for workspace ↔ remote sync.

| Method | Mechanism |
|--------|-----------|
| `pushDirectory(local, remote)` | `rsync -avz --delete --progress` |
| `pullDirectory(remote, local)` | `rsync -avz --progress` |
| `uploadFile(local, remote)` | `scp` |
| `downloadFile(remote, local)` | `scp` |
| `cancel()` | Kill running process |

Uses `-e ssh [options]` for rsync SSH transport with custom port/key support.

### Remote build (OutputPanel extension)

`OutputPanel::runRemoteCommand(host, port, user, keyPath, cmd, workDir)` runs
build commands on the remote host via SSH with streaming stdout/stderr and
problem matching (same matchers as local builds).

## MainWindow integration

- **Remote dock** — tabified with Project dock (left side), hidden by default
- **View → Remote / SSH** — toggle action with safeVisSync
- **Double-click remote file** → download via `SshSession::readFile()` → temp file → `openFile()`
- **Remote terminal** → `TerminalPanel::addSshTerminal()` opens interactive SSH session
- **Architecture auto-detection** — probed on connect, persisted in profile, shown in combo and panel

## Extension points

- New architecture support: add `CpuArch` enum value + `archFromUname()` case + `targetTriple()` mapping
- New toolchain detection: add to the `command -v` loop in `RemoteHostProber::probe()`
- Custom sync strategies: subclass or extend `RemoteSyncService`
