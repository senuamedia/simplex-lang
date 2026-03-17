# Security Policy

## Supported Versions

| Version  | Supported |
|----------|-----------|
| 0.14.x   | Yes       |
| 0.13.x   | Yes       |
| 0.12.x   | No        |
| < 0.12   | No        |

## Reporting a Vulnerability

If you discover a security vulnerability in Simplex, please report it
responsibly by emailing **security@senuamedia.com**.

Do not open a public issue for security vulnerabilities.

Your report should include:

- A clear description of the vulnerability
- Steps to reproduce the issue
- An assessment of the potential impact
- Any suggested mitigation, if applicable

We will acknowledge receipt of your report within **72 hours** and provide
an initial assessment within 7 business days.

## Scope

The following components are in scope for security reports:

- Simplex compiler (`sxc`)
- Standalone runtime (`standalone_runtime.c`)
- Standard library (`lib/`)
- Nexus protocol (`simplex-nexus/`)
- Edge Hive (`simplex-edge-hive/`)
- Quantum framework (`simplex-quantum/`)

The following are out of scope:

- Third-party dependencies
- Example code and tutorials
- Documentation site

## Disclosure Policy

We follow a coordinated disclosure process:

1. The reporter submits the vulnerability privately.
2. Our team confirms and develops a fix.
3. A patched release is issued before public disclosure.
4. Public disclosure occurs no later than **90 days** after the initial report.

We request that reporters refrain from public disclosure until a fix is
available or the 90-day window has elapsed, whichever comes first.

## Contact

- Security reports: security@senuamedia.com
- Project: [github.com/senuamedia/simplex-lang](https://github.com/senuamedia/simplex-lang)
- Maintainer: Senua Media
