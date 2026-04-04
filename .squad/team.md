# Squad Team

> supercell-wx

## Coordinator

| Name | Role | Notes |
|------|------|-------|
| Squad | Coordinator | Routes work, enforces handoffs and reviewer gates. |

## Members

| Name | Role | Charter | Status |
|------|------|---------|--------|
| Ripley | Lead | .squad/agents/ripley/charter.md | Active |
| Bishop | Core C++ Dev | .squad/agents/bishop/charter.md | Active |
| Parker | Qt/UI Dev | .squad/agents/parker/charter.md | Active |
| Hudson | Tester | .squad/agents/hudson/charter.md | Active |
| Vasquez | Build & Release | .squad/agents/vasquez/charter.md | Active |
| Scribe | Session Logger | .squad/agents/scribe/charter.md | Active |
| Ralph | Work Monitor | .squad/agents/ralph/charter.md | Active |


## Coding Agent

<!-- copilot-auto-assign: false -->

| Name | Role | Charter | Status |
|------|------|---------|--------|
| @copilot | Coding Agent | — | 🤖 Coding Agent |

### Capabilities

**🟢 Good fit — auto-route when enabled:**
- Bug fixes with clear reproduction steps
- Test coverage (adding missing tests, fixing flaky tests)
- Lint/format fixes and code style cleanup
- Dependency updates and version bumps
- Small isolated features with clear specs
- Boilerplate/scaffolding generation
- Documentation fixes and README updates

**🟡 Needs review — route to @copilot but flag for squad member PR review:**
- Medium features with clear specs and acceptance criteria
- Refactoring with existing test coverage
- API endpoint additions following established patterns
- Migration scripts with well-defined schemas

**🔴 Not suitable — route to squad member instead:**
- Architecture decisions and system design
- Multi-system integration requiring coordination
- Ambiguous requirements needing clarification
- Security-critical changes (auth, encryption, access control)
- Performance-critical paths requiring benchmarking
- Changes requiring cross-team discussion

## Project Context

- **Owner:** Dan Paulat
- **Project:** supercell-wx
- **Product:** Cross-platform application for visualizing live and archived NEXRAD weather radar data
- **Stack:** C++20, Qt6, CMake, Conan 2, GoogleTest, Python build helpers
- **Cast Universe:** Alien
- **Created:** 2026-04-04
