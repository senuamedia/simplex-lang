# TASK-026: Authentication & Security Libraries

**Status**: Planning
**Priority**: High
**Created**: 2026-03-16
**Target Version**: 0.16.0
**Depends On**: TASK-022 Phase 8 (HTTP client, JSON parser)

---

## Overview

Authentication and security libraries required for any user-facing application or API service. Simplex already has `simplex-std/src/crypto.sx` (SHA256, bcrypt, token generation) — these libraries build on that foundation.

All implementations must be **pure Simplex** using the existing crypto intrinsics in the runtime.

---

## Library 1: simplex-jwt

**Location**: `simplex-std/src/jwt.sx`
**Priority**: Critical — every API needs token-based auth

### Core API
```simplex
struct JwtHeader {
    alg: String,    // "HS256", "RS256"
    typ: String     // "JWT"
}

struct JwtPayload {
    claims: Vec<JwtClaim>
}

struct JwtClaim {
    key: String,
    value: JsonValue
}

fn jwt_encode(payload: JwtPayload, secret: String, alg: String) -> Result<String, JwtError>
fn jwt_decode(token: String, secret: String) -> Result<JwtPayload, JwtError>
fn jwt_verify(token: String, secret: String) -> Result<bool, JwtError>
```

### Features
- **Algorithms**: HMAC-SHA256 (HS256) — primary; RS256 as stretch goal
- **Standard claims**: `iss`, `sub`, `aud`, `exp`, `nbf`, `iat`, `jti`
- **Expiration checking**: Auto-reject expired tokens
- **Base64url encoding/decoding**: Required for JWT format
- **Claim extraction**: Type-safe access to individual claims

### Success Criteria
- Generate and verify HS256 tokens
- Expired tokens are rejected
- Tampered tokens fail verification
- Standard claims parse correctly
- Interoperable with tokens from other JWT libraries
- New test: `tests/stdlib/spec_jwt.sx`

---

## Library 2: simplex-oauth

**Location**: `simplex-oauth/src/mod.sx`
**Priority**: High — auth with Google, GitHub, Microsoft, etc.

### Core API
```simplex
struct OAuthConfig {
    client_id: String,
    client_secret: String,
    auth_url: String,
    token_url: String,
    redirect_uri: String,
    scopes: Vec<String>
}

struct OAuthToken {
    access_token: String,
    token_type: String,
    expires_in: i64,
    refresh_token: Option<String>,
    scope: String
}

fn oauth_auth_url(config: OAuthConfig, state: String) -> String
fn oauth_exchange_code(config: OAuthConfig, code: String) -> Result<OAuthToken, OAuthError>
fn oauth_refresh(config: OAuthConfig, refresh_token: String) -> Result<OAuthToken, OAuthError>
fn oauth_validate(token: OAuthToken) -> bool
```

### Features
- **OAuth 2.0 Authorization Code flow** (the standard web flow)
- **PKCE extension** (Proof Key for Code Exchange — required for public clients)
- **Token refresh** with automatic expiry detection
- **Provider presets**: Pre-configured OAuthConfig for common providers
  ```simplex
  fn oauth_github(client_id: String, client_secret: String) -> OAuthConfig
  fn oauth_google(client_id: String, client_secret: String) -> OAuthConfig
  ```
- **State parameter** for CSRF protection

### Dependencies
- `simplex-std/src/http_client.sx` — for token exchange HTTP POST
- `simplex-std/src/json.sx` — for parsing token response
- `simplex-std/src/crypto.sx` — for PKCE code_verifier/code_challenge

### Success Criteria
- Generate valid authorization URL with state and PKCE
- Exchange authorization code for access token
- Refresh expired token
- Provider presets produce correct URLs
- New test: `tests/stdlib/spec_oauth.sx`

---

## Library 3: simplex-dotenv

**Location**: `simplex-std/src/dotenv.sx`
**Priority**: Medium — config management for development and deployment

### Core API
```simplex
fn dotenv_load(path: String) -> Result<Vec<EnvEntry>, DotenvError>
fn dotenv_load_default() -> Result<Vec<EnvEntry>, DotenvError>  // loads .env from cwd
fn dotenv_apply(entries: Vec<EnvEntry>)  // sets environment variables

struct EnvEntry {
    key: String,
    value: String
}
```

### Features
- Parse `.env` file format (KEY=VALUE, one per line)
- Quoted values (single and double quotes)
- Comment lines (`#`)
- Variable expansion (`${VAR}` references within values)
- Multiline values (with quotes)
- Don't override existing environment variables (existing env takes precedence)

### Success Criteria
- Parse standard `.env` files
- Quoted values preserve spaces
- Comments are ignored
- Variable expansion resolves correctly
- Existing env vars are not overwritten
- New test: `tests/stdlib/spec_dotenv.sx`

---

## Dependency Graph

```
TASK-022 Phase 8 (HTTP + JSON) + simplex-std/src/crypto.sx (exists)
    |
    +--> simplex-jwt (crypto + base64url + JSON)
    |         |
    |         v
    +--> simplex-oauth (HTTP client + JSON + JWT optional)
    |
    +--> simplex-dotenv (independent, file I/O only)
```

simplex-dotenv is fully independent. simplex-jwt first, then simplex-oauth.

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-jwt | ~600-800 |
| simplex-oauth | ~800-1,000 |
| simplex-dotenv | ~200-300 |
| **Total** | **~1,600-2,100** |
