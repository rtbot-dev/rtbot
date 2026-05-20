# rtbot JIT — deployment requirements

The rtbot JIT path requires the runtime to allocate executable memory pages
(`mprotect(PROT_EXEC)` or platform equivalent). Most customer environments
support this by default, but several common hardened configurations block it.
This document lists the known cases and the resolution per platform.

When the JIT cannot run, rtbot automatically falls back to the FE bytecode
interpreter — except for the macOS-hardened-runtime SIGKILL case noted below,
which is detected at code-sign time and must be resolved before deployment.

## Quick check at deploy time

After deploying rtbot, verify the JIT is active:

- Look at rtbot's stderr / log at process startup. The `Program(json)` constructor
  attempts a JIT compile on first use; on failure it logs a warning and
  silently uses the interpreter.
- For test deployments, build with `-DRTBOT_JIT_LOG_BACKEND=1` (TBD if rtbot
  exposes such a flag) or call the debug accessor `Program::using_jit()` from
  an integration test to confirm the JIT path is active.

If JIT is silently disabled in your environment, rtbot still produces correct
output — just slower. The interpreter path is the deployment fallback for
every platform regardless of how restrictive the runtime is.

## macOS

### Default macOS runtime — works out of the box

No configuration needed. The kernel allows `mprotect(PROT_EXEC)` on
arbitrary memory pages for ordinary processes.

### Hardened runtime + notarized .app or .modl — REQUIRES allow-jit entitlement

If your distribution flow requires notarization (Mac App Store, Developer ID
+ notarytool) the binary is signed with the hardened runtime (`--options runtime`).
Under hardened runtime, the kernel sends **`SIGKILL`** the moment the JIT
allocates an executable page — there is no recoverable fault, the process
dies. **rtbot's runtime fallback cannot detect this case** because the kernel
kills the process before any exception can fire.

**Resolution**: include the `com.apple.security.cs.allow-jit` entitlement at
codesign time. Example entitlements file:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>com.apple.security.cs.allow-jit</key>
  <true/>
</dict>
</plist>
```

Sign with:

```bash
codesign --force --options runtime \
  --entitlements rtbot-entitlements.plist \
  --sign "Developer ID Application: <your-team>" \
  <binary>
```

If you ship rtbot inside another binary (e.g. an Ignition module bundle), the
**outer binary** is what needs the entitlement. The Mac App Store reviews
allow-jit applications — declare the JIT use in your app description.

JavaScript engines (V8, JavaScriptCore), .NET, the JVM, and emulators (UTM,
Parallels) all use this same entitlement. It is a normal, well-understood
deployment path; Apple is fine with it as long as JIT use is declared.

## Linux

### Default Linux — works out of the box

`mprotect(PROT_EXEC)` is allowed by default on most distributions.

### SELinux enforcing — needs the `execmem` boolean

Many production Linux servers run SELinux in enforcing mode. The default
policy denies arbitrary `mprotect(PROT_EXEC)` for unconfined processes.
rtbot's JIT will fail with `LLVM ERROR: Allocation of executable memory failed`,
the constructor will catch the exception, and the process will fall back to
the interpreter — correct but slow.

**Resolution (system-wide)**: enable the `execmem` boolean.

```bash
sudo setsebool -P execmem 1
```

**Resolution (targeted)**: write a custom SELinux policy for the rtbot service
account that grants it `execmem`. This is more work but avoids weakening the
system-wide policy. Consult your security team.

### AppArmor enforcing — needs profile relaxation

Default AppArmor profiles on Ubuntu/Debian deny `m` (memory map executable)
on processes they cover. The JIT will fail with the same error as SELinux
above and rtbot will fall back to the interpreter.

**Resolution**: edit the profile that confines rtbot (or its parent process,
e.g. Ignition Gateway under `/etc/apparmor.d/usr.lib.ignition.gateway`) to
allow `m` on relevant memory regions:

```
# In the profile body
mr,    # allow read+exec memory mappings
```

Or run the rtbot binary in a permissive context. Reload AppArmor after
editing.

### Hardened systemd unit (`MemoryDenyWriteExecute=yes`) — explicitly forbids JIT

Some hardened Linux services include `MemoryDenyWriteExecute=yes` in their
systemd unit file. This unconditionally blocks executable memory mapping.

**Resolution**: remove the `MemoryDenyWriteExecute=yes` line from the unit
file (or set to `no`), then `systemctl daemon-reload && systemctl restart <service>`.
If you cannot edit the unit (e.g. it's part of a vendor package), override
with a drop-in:

```
# /etc/systemd/system/<service>.service.d/rtbot-jit.conf
[Service]
MemoryDenyWriteExecute=no
```

## Windows

### Default Windows — works out of the box

DEP (Data Execution Prevention) is enforced by default on modern Windows but
allows JIT memory by following the standard pattern: `VirtualAlloc(...,
PAGE_EXECUTE_READWRITE)` followed by `VirtualProtect` to read+exec. LLVM
ORC's allocator uses this pattern.

### Antivirus interactions

Some endpoint security tools (corporate antivirus, EDR agents) flag JIT-allocated
pages as suspicious behavior — same heuristic that flags V8 / .NET / JVM JITs.
**Resolution**: whitelist the rtbot binary in the security tool's configuration.
This is an operational requirement at deployment time; rtbot has no runtime
workaround.

### Hardened Windows configurations

Some hardened Windows deployments use Code Integrity policies (Device Guard /
HVCI) that deny JIT pages. The behavior is identical to SELinux: rtbot's
JIT compile fails, the exception is caught, and the interpreter takes over.

## Runtime fallback summary

For all non-fatal failures (anything except macOS hardened-runtime SIGKILL),
rtbot's `Program` class automatically falls back to the FE interpreter. The
fallback decision is made:

- **Per-process**: The startup probe (`probe_runtime_support()`) runs once
  per process and caches the result. If it fails, every subsequent
  `Program(json)` skips the JIT path.
- **Per-program**: Even when JIT is generally available, a specific JSON may
  use unsupported opcodes (e.g. `Demux`, `Mux`, `Pipeline` — out of scope for
  the current JIT). Each `Program(json)` tries to compile; on failure for
  that specific JSON, only that program falls back.

Both fallbacks are silent (no exceptions thrown to the caller). If you need
to assert that JIT is active in a deployment, use the debug accessor
`Program::using_jit()` from an integration test.

## Observed environments and their resolutions

| Environment | Result | Resolution |
|---|---|---|
| macOS default (development) | OK | No configuration needed |
| macOS hardened runtime, no entitlement | SIGKILL | Add `com.apple.security.cs.allow-jit` at sign time |
| macOS hardened runtime, with entitlement | OK | Working |
| Linux default (Ubuntu/Debian/Arch desktop) | OK | No configuration needed |
| Linux + SELinux enforcing | BLOCKED → falls back | `setsebool -P execmem 1` for full perf |
| Linux + AppArmor (default Ubuntu) | BLOCKED → falls back | Profile relaxation or unconfined |
| Linux + systemd `MemoryDenyWriteExecute=yes` | BLOCKED → falls back | Remove the directive |
| Windows 10/11 default | OK | No configuration needed |
| Windows + corporate AV | OK / FLAGGED | May require whitelisting |
| Ignition Gateway (Linux) | Inherits Gateway service config | See SELinux/AppArmor sections |
| Ignition Gateway (macOS) | Inherits Gateway codesign | See macOS hardened runtime |

## Customer FAQ

**Q: How do I tell if rtbot is using JIT or the interpreter?**

A: Build with debug logging (TBD specific flag), or call `Program::using_jit()`
from an integration test. The interpreter is functionally identical to the JIT —
same outputs, just slower.

**Q: How much slower is the interpreter?**

A: Depending on the program shape, the interpreter is ~10× slower than JIT for
math-heavy pipelines (Bollinger ~7M msgs/s interpreter vs ~67M msgs/s JIT on a
modern Apple Silicon dev box). For routing/IO-bound pipelines the gap is smaller.

**Q: Can I force the interpreter even when JIT is available?**

A: Not currently exposed via a public flag. To force the interpreter, deploy in
an environment that fails the startup probe (e.g. unset the `execmem` boolean
on SELinux). For testing, modify the rtbot binary to skip the JIT compile.

**Q: Does using JIT change the output of my pipeline?**

A: No. The JIT and interpreter produce **bit-exact identical output** for every
supported pipeline shape, validated by the rtbot test suite (~50,000 parity
assertions covering all supported opcodes and end-to-end Bollinger and PPG
shapes). FP-determinism is preserved across both paths.

**Q: My deployment shows JIT is unavailable. Is that a security risk?**

A: No — the interpreter is fully functional and produces correct output. The
only impact is throughput. Address the environment configuration if performance
matters for your workload; otherwise the interpreter is a fine permanent state.
