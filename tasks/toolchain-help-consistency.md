# Toolchain Help Command Consistency

**Status:** Planned
**Priority:** Low
**Version:** Post-0.5.1

## Overview

Standardize help command interface across all Simplex toolchain binaries for consistent user experience.

## Current State

| Tool | `<tool> help` | `<tool> -h` | `<tool> --help` |
|------|---------------|-------------|-----------------|
| sxc | - Done Works | - Done Works | - Done Works |
| sxpm | No "Unknown command" | - Done Works | - Done Works |
| sxdoc | No No output | - Done Works | - Done Works |
| cursus | No No output | - Done Works | - Done Works |
| sxlsp | No No output | - Done Works | - Done Works |

## Proposed Standard

All tools should support **all three** help invocations:
1. `<tool> help` - subcommand style (like git, cargo)
2. `<tool> -h` - short flag
3. `<tool> --help` - long flag

## Implementation

### Files to Modify

- `tools/sxpm.sx` - Add `help` command recognition
- `tools/sxdoc.sx` - Add `help` command recognition
- `tools/cursus.sx` - Add `help` command recognition
- `tools/sxlsp.sx` - Add `help` command recognition

### Code Pattern

Each tool's argument parsing should include:

```simplex
// In parse_args or main
if string_eq(cmd, "help") {
    show_help();
    return 0;
}
```

### Testing

After implementation, verify:
```bash
sxc help && sxc -h && sxc --help
sxpm help && sxpm -h && sxpm --help
sxdoc help && sxdoc -h && sxdoc --help
cursus help && cursus -h && cursus --help
sxlsp help && sxlsp -h && sxlsp --help
```

## Additional Improvements (Optional)

1. **Version consistency**: Ensure all tools show version with `--version` and `version` subcommand
2. **Color output**: Add ANSI color to help output for better readability
3. **Man pages**: Generate man pages from help text
4. **Shell completions**: Generate bash/zsh completions

## Acceptance Criteria

- [ ] All five tools support `help` subcommand
- [ ] All five tools support `-h` flag
- [ ] All five tools support `--help` flag
- [ ] Help output format is consistent across tools
- [ ] Tests pass for all help invocations
