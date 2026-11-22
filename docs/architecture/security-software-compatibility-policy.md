# VGK security software compatibility policy (proposal)

**Status:** proposed design; not an implementation or a claim that the security subsystem is complete.

## 1. Goal

VGK should provide a strong built-in security baseline. A user should not need to install a traditional third-party antivirus to obtain the operating system's basic protection.

At the same time, VGK should not prohibit every additional security product. Administrators and organizations may have legitimate needs for independent malware scanners, endpoint detection and response (EDR), forensic tools, or centrally managed security agents.

The compatibility boundary should be based on privileges and integration method, not merely on whether software is called an antivirus.

## 2. Proposed policy

1. **Built-in protection is always present.** Core protections are part of VGK and are maintained with the operating system.
2. **No unrestricted third-party kernel access.** Ordinary security applications must not install arbitrary kernel code, patch the kernel, hook undocumented internals, or write privileged CPU control registers.
3. **Kernel extensions are exceptional.** Any future kernel extension mechanism must require explicit authorization, publisher verification, compatibility checks, a narrow documented interface, and a recovery path. A security-product label alone grants no exception.
4. **User-space security tools may be supported.** Additional scanners and endpoint agents may use documented, least-privilege interfaces in a sandbox.
5. **The system owns enforcement.** Third-party tools may report, scan, request quarantine, or request a policy action; only the VGK security authority decides whether a privileged action is permitted.
6. **No silent disabling of protections.** An application must not silently turn off Secure Boot verification, code-signing checks, isolation, updates, or other core safeguards.

## 3. Future supported security interfaces

The following are design candidates, not implemented APIs:

- File inspection interface for on-demand and scheduled scanning.
- File reputation and download inspection service.
- Execution authorization / pre-launch inspection notifications.
- Quarantine request interface controlled by VGK.
- Network filtering interface with explicit capabilities and resource limits.
- Security event stream with privacy controls and scoped access.
- Administrative policy interface for managed devices.

Interfaces should provide stable, versioned contracts and must not expose unrestricted kernel memory or arbitrary process modification.

## 4. Installation and compatibility behavior

The installer should distinguish at least three cases:

- **Supported user-space security application:** may be installed after signature, permission, and compatibility checks.
- **Security application requesting unsupported privileged access:** installation of that component is denied with a clear explanation; a supported user-space mode may still be offered.
- **Untrusted or tampered application:** blocked by the normal VGK trust and execution policy, regardless of its advertised purpose.

VGK should not display a blanket message that antivirus software is forbidden. It should explain which requested permission or integration method is unsupported.

## 5. Built-in protection responsibilities

The VGK security architecture should be designed to cover, at minimum:

- verified boot and integrity of trusted startup components;
- signed and verified system updates;
- application identity, publisher trust, and execution policy;
- process isolation and least privilege;
- memory and kernel integrity protections;
- file and removable-media inspection hooks;
- firewall and network policy;
- security event logging and user-visible alerts;
- recovery and rollback after a failed or malicious update.

These are architectural requirements, not statements that every mechanism already exists in the current prototype.

## 6. Performance, privacy, and user control

Security services should be resource-aware, especially on systems with 4–8 GB RAM. Scanning should support scheduling, exclusions with clear warnings, and bounded resource use.

Telemetry should be minimized and disclosed. Local protection must continue to function without a cloud account or paid subscription. Managed-device reporting, if added, must be explicit and policy-controlled.

## 7. Safety and validation requirements

Before any security extension or privileged interface is implemented, define:

- threat model and trust boundaries;
- capability and permission model;
- failure behavior and recovery path;
- compatibility and versioning rules;
- resource limits and denial-of-service protections;
- unit, integration, negative, and fuzz tests;
- tests proving that a third-party component cannot bypass kernel protections.

## 8. Open design decision

The proposed default is **built-in protection first, no unrestricted third-party antivirus kernel drivers, but controlled support for compatible user-space security tools**.

This proposal does not authorize third-party kernel modules, does not implement an antivirus engine, and does not claim VGK is currently protected against all malware.
