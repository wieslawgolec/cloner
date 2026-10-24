# cloner
                    
                    	     _/_/_/  _/
                    	  _/        _/    _/_/    _/_/_/      _/_/    _/  _/_/
                    	 _/        _/  _/    _/  _/    _/  _/_/_/_/  _/_/
                    	_/        _/  _/    _/  _/    _/  _/        _/
                    	 _/_/_/  _/    _/_/    _/    _/    _/_/_/  _/
              simple, unoptimized, maybe buggy irc cloner program BUT IT ROCKS.

IRC channel cloner for IRCNet-style networks. Based on [joco] cloner (jococloner).

It is a small, non-blocking C client that connects multiple clones, joins a channel, and coordinates ops / kicks while respecting ircd flood penalties.

**Version:** 0.3.0  
**License:** [GNU General Public License v2.0 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html) (same as the original jococloner)

---

## Features

- Native 64-bit builds (Linux, macOS, FreeBSD); optional 32-bit
- Optional OpenSSL TLS (port 6697 / IRCNet 2.11+ / 2.12.0-pre)
- IPv6 support
- Built-in identd (privileged port 113)
- Vhost / bind-IP rotation from a file
- Fast / normal work modes and kick vs deop punish modes
- Optional binary integrity validator
- GitHub Actions CI (Ubuntu, macOS, FreeBSD)

Plain TCP behaviour is unchanged; SSL is additive.

---

## Requirements

- C compiler (`gcc` or compatible)
- `make` / `gmake`
- Optional: OpenSSL development package (`libssl-dev`, `openssl`, etc.) for TLS
- Optional: `pkg-config` (helps locate OpenSSL)

---

## Build

```bash
cd src
./configure [options]
make dynamic          # recommended
```

The binary is installed as `../cloner-<version>-<os>` with a `../cloner` symlink.

### Configure options

| Option | Description |
|--------|-------------|
| `--with-ssl` | Force OpenSSL (error if not found) |
| `--without-ssl` | Disable TLS |
| `--with-ipv6` | Enable IPv6 (default on most platforms) |
| `--without-ipv6` | Disable IPv6 |
| `--with-32bit` | Force `-m32` (needs multilib) |
| `--without-validator` | Do not build/use the binary protector |
| `--help` | Show help |

SSL is **auto-detected** when neither `--with-ssl` nor `--without-ssl` is given.

### Other make targets

```bash
make static     # static binary where supported
make debug      # -DDEBUG -ggdb
make release    # release tarball (with validator when enabled)
make clean
make src        # source package
```

### Examples

```bash
# Default (auto SSL if OpenSSL is present)
./configure && make dynamic

# Explicit SSL, no validator
./configure --with-ssl --without-validator && make dynamic

# Plain TCP only
./configure --without-ssl && make dynamic
```

---

## Usage

```text
./cloner <server> [port] [number] [delay] [vhost-file]
```

| Argument | Default | Meaning |
|----------|---------|---------|
| `server` | (required) | Hostname or IP |
| `port` | `6667` | Server port. **`6697` enables TLS** when built with SSL |
| `number` | `0` | How many clones to connect at start |
| `delay` | `1` | Seconds between successive connects |
| `vhost-file` | `.vhosts` if present | File of bind IPs (one per line) |

Examples:

```bash
./cloner irc.example.net
./cloner irc.example.net 6667 20 2
./cloner irc.example.net 6697 10 1 my-vhosts.txt   # TLS
```

Create `.no-ident` in the working directory to skip the built-in ident listener.

---

## Console commands

Type commands on stdin after clones are connected. Use `help` for a short list.

| Command | Arguments | Description |
|---------|-----------|-------------|
| `help` | — | List commands |
| `chan` | `[#channel]` | Set or show target channel. Changing channel parts the previous one |
| `key` | `[key]` | Set or show channel key |
| `join` | `[delay]` | Join the channel. Optional delay (seconds) between clones |
| `part` | — | Part the channel |
| `load` | `<n>` | Connect *n* additional clones |
| `addop` | `<nick>` | Protect nick (will not be deopped/kicked by clones) |
| `delop` | `<nick>` | Remove nick from op list |
| `listop` | — | Show protected ops |
| `deop` | — | Deop all clones on the channel |
| `fast` | `[on\|off]` | Fast mode: do not wait for MODE replies (faster, riskier when lagged) |
| `mode` | `[kick\|deop]` | Punish mode for non-protected ops |
| `lag` | — | Show penalty / order-check status |
| `quit` | — | QUIT all clones and exit |

When built with author-check support, `neutral [on|off]` is also available.

---

## SSL / TLS

- Build with OpenSSL (`--with-ssl` or auto-detect).
- Port **6697** turns on TLS automatically.
- Non-blocking `SSL_connect` / `SSL_read` / `SSL_write` in the main event loop.
- Certificate verification is currently disabled (typical for this class of tool).
- IRCNet may send a NOTICE such as “Your connection is secure (SSL/TLS)” and a WHOIS “secure connection” flag.

Plain TCP on other ports is unchanged.

---

## Notes

**Fast mode** assumes a MODE was applied as soon as it is sent. With low lag this is very fast; with high lag the clone army can get out of sync.

On small/local networks, ircd often processes the **last** connected client first (reverse of connect order). For fair tests, start competing cloners at similar times and delays.

The program does not fully model channel modes, bans, or nick collisions; it assumes joins and nicks generally succeed.

---

## License

Copyright (C) original jococloner authors and subsequent contributors.

This program is free software; you can redistribute it and/or modify it under the terms of the **GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version**.

This program is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <https://www.gnu.org/licenses/>.

Based on jococloner by [joco] of --sirk-labs--.

---

## Changelog

See [CHANGELOG](CHANGELOG) for full history.
