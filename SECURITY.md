# Security policy

This is an embedded-focused state-machine library with no network I/O. It
does parse portable snapshot bytes, but the decoder is strict about size,
version, reserved fields, and state compatibility.

If you believe you have found a security issue, please **do not** post
potentially sensitive details in a public GitHub issue.

## Reporting

- Preferred: use **GitHub Security Advisories** (“Report a vulnerability”) if
  available for this repository.
- Otherwise: contact the maintainer privately (for example via GitHub direct
  message) to coordinate disclosure.

Include a minimal reproducer, expected vs actual behavior, and compiler/target
details.
