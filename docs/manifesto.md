# Exorcist Manifesto

This document is the project's spine. Every major decision must align with it.

## 1) Modularity Over Monolith
- Core interfaces define the system; implementations are replaceable.
- UI, features, and plugins depend only on stable APIs.
- No internal headers in plugins.

## 2) Minimal Footprint
- Default to lean dependencies.
- Prefer lazy loading and on-demand services.
- Measure memory and startup time continuously.

## 3) Cross-Platform First
- Every feature must be portable by design.
- Platform code stays isolated behind core bindings.
- No hard OS assumptions in UI or feature layers.

## 4) Editor Architecture Done Right
- Separate document model from views and UI state.
- Keep the text engine incremental and fast.
- Prefer proven parsers (e.g., tree-sitter) over ad-hoc regex.

## 5) Intellisense Without Chaos
- LSP is the default contract.
- Language tooling runs out-of-process when possible.
- Clear lifecycle management: start/stop/timeout/restart.

## 6) Layered Ownership (RAII)
- Ownership is explicit and local.
- Qt ownership for UI; smart pointers for core/services.
- Avoid global singletons.

## 7) Self-Hosting Early
- Build + run + debug inside Exorcist ASAP.
- Dogfooding drives architecture improvements.

## 8) Stable Plugin SDK
- Small, versioned interfaces.
- Backward compatibility is a core requirement.

## 9) Transparent Defaults
- No hidden network access.
- No telemetry by default.

## 10) Quality and Longevity
- Clear code, minimal magic.
- Tests and tooling grow alongside features.
