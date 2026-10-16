[English](login-flow.en.md) | [中文](login-flow.md)

# Apollo Login Flow Overall Design (login-flow)

> Status: design draft (under review). Positioning: **the complete login chain from client launch to entering the world** — account authentication, login_token issuance/verification/redemption, server selection/queueing/admission, contract pack delivery timing, and the registration interlock with the online directory. Gap registered: design-gap-inventory #13 (B11 batch, set down 2026-10-01). Cross-references: net-abstraction §5.9 (login_token/handshake family), §7 (G-1/baseappmgr admission), §5.7 (InterServerLink); session-and-online-directory (#12 — registration anchor/displacement/keep-alive window, this design's direct downstream); sdk-contract §11 (message domains)/§11.6 (contract pack); attribute-sync §8.2 (storage.xml account tables); scripting-lua §7.5 (GM kick execution point); inbound-interfaces (#14 same batch — account binding, same domain); capacity-and-benchmark (surge capacity); architecture-review §25 (legacy read)/§26 (six design-batch input questions).

---

## Executive Summary

1. **Two-phase connection; login-app is an independent process that does not go through gateway**: phase one = the login connection (client ↔ login-app, independent port, short-lived, disconnects once authentication and token issuance complete); phase two = the game connection (client ↔ gateway-app, ClientHello carries login_token — the existing §5.9 semantics). "Topology entry = gateway" refers to the game session plane; the login connection is the preceding short plane (the same-form twenty-year precedent of BW/KBE independent loginapp processes, No. 36 :55/:66). Rationale: the account-credential sensitive surface never enters gateway; login/reconnect surges are isolated in fault domain and capacity domain from the four-channel game traffic (a CCU storm hits login-app, not gateway); login-app holds no session state and scales horizontally.
2. **The login connection is encrypted too (the LoginHello handshake family)**: §5.9's ClientHello **carries a login_token** (handshake only once the token is in hand) — where the token comes from is the chicken-and-egg gap this design fills: the login connection first does an **anonymous X25519 handshake** (no token; credentials travel inside AEAD); the algorithm family is exactly the same lineage as §5.9 (X25519 + HKDF-SHA256, info = `"apollo-login-v1"` domain separation); credentials never cross in plaintext (KBE loginapp's bare-TCP plaintext is an old-engine chronic ailment; not imitated).
3. **login_token = a self-contained HMAC-SHA256 ticket + one-time redemption in the baseappmgr admission critical section**: payload = {account_id, issue/exp_tick, nonce, gateway_id, zone_hint, purpose}; short TTL (60s default recommendation, calibrated in the capacity batch); gateway **verifies the signature locally** (does not touch login-app/the account store — fulfilling the §5.9 :313 intent; the legacy gateway RPC round-trip is a misplaced implementation, migration belongs to a code batch); **the one-time redemption point = the baseappmgr admission critical section** (the pending admission table records the nonce; SessionUp converts it to a formal entry — obtained for free in the same critical section as the #12 registration anchor, zero new state surface; replay = nonce mismatch rejection + violation_score).
4. **Authentication belongs to login-app; admission/server selection/displacement/queueing belong to baseappmgr (single-point serialization)**: after login-app authenticates, it sends one `AdmissionRequest` (RequestReply low-frequency control plane) → four steps inside baseappmgr's critical section: load gate (LoginConditions same form, net §7 :389/:395) → displacement pre-ruling (#2 §3 critical section: old entry Removed + kick delivery) → lightest-placement baseapp (minAppLoad same form) → pending table records nonce → returns {gateway_addr, zone_hint, world_assignment initial value}. login-app has no directory and no load table (the legacy GatewayAllocator's gateway-selection duty merges into baseappmgr — the global view lives only in baseappmgr).
5. **Account domain = DB tables (batch 2 storage.xml statement set) + P1 in-memory degradation**: accounts table (account_id/credential_salt/credential_hash/state/banned_until/last_login) + third_party_bindings table (#14 shares the same domain); credential hashing = PBKDF2-HMAC-SHA256 (OpenSSL built-in — §5.9's single crypto source stays intact; no bcrypt/argon2 introduced); P1 degradation = an in-process test account table (the legitimate degenerate form of the legacy :22-41 hardcoded shape; plaintext comparison banned). Risk-control consumption: violation_score disconnection + account-level risk-control records (§4.3 :128) checked at the admission gate (banned_until).
6. **Queueing = a P3 component; P1-P2 rejection codes**: overload returns busy + retry_after (BW LoginConditions is itself rejection-style, KBE the same — queueing is not built into either); P3 = a baseappmgr-side admission queue (watermark-driven, queue_position events delivered via login-app, message slot reserved). "Queueing is a CN-MMO-specific need, absent from both precedents" — designed as a component, not a default.
7. **Contract pack delivery = pointer-based; login-app does no file serving**: the login response carries {client_hash, bundle_url}; the client compares against its local manifest (§11.6 three-piece set, per-file SHA-256), and on mismatch fetches via bundle_url — CI contract_pack already produces `dist/client/<client_hash>/` (sdk-contract §12.2). KBE clientsdk_downloader's "engine as CDN" form is not adopted (apollo has a CI packaging pipeline; the engine ships pointers, not bytes); P1 degradation = configured static URL.
8. **The whole chain interlocks with #12**: authentication (login-app) → admission + displacement pre-ruling + placement (baseappmgr critical section) → token issuance (login-app) → ClientHello signature verification (gateway) → baseapp creates the session entity → SessionUp → directory visible (#2 §2 anchor) — **the pre-admission chain belongs to this design; post-admission registration/window/displacement execution belongs to #12**; the boundary is the declared line at #12 :71. The legacy login-app 884-line stovepipe (§25 read: four misalignments) is not overthrown: it is positioned as the P1 demo piece; the migration surface is registered item by item in §10 for a code batch.

## 0. Status Quo and Gap

- **Design side**: net-abstraction §5.9 has only one attribution line for login_token (:313) and the handshake sequence (:302-311, premised on the token already in hand); server selection/queueing/admission gets only half a sentence on the baseappmgr gate (§7 :395); SDK delivery timing has zero design — gap #13's three evidence items pass in one sentence (review in architecture-review §25).
- **Legacy side (§25 read)**: apps/login-app is an 884-line zero-framework stovepipe — hardcoded plaintext authentication (login_server.cpp:27 self-notes it should use a hash), self-made xorshift PRNG tickets (:284-301), sliding renewal instead of one-time (:245, conflicting with "short TTL + one-time"), preparePlayerOnline calling base three times directly, bypassing gateway/baseappmgr, with catch swallowing exceptions into fake success (:476-542/:536-541); gateway ingress admission = RPC round-trip to login-app (session_admission_service.cpp:33) instead of signature verification. The four misalignments = hard evidence for gap criterion (b), legacy masquerading; **migration belongs to a code batch (§10), source frozen untouched**.
- **Precedents**: BW = the client first connects to loginapp and gets a baseapp assigned (No. 36 :55); the mgr holds load-based routing (baseappmgr :1117 reports to loginapp); KBE = loginapp as an independent process (No. 36 :66) + clientsdk_downloader SDK delivery (deep-dive :100); neither has built-in queueing.

## 1. Division of Duties and Topology

```text
Phase one (login connection, short)         Phase two (game connection, long)
Client ──LoginHello/LoginAuth──► login-app    Client ──ClientHello{login_token}──► gateway-app
        ◄─LoginResponse─┘          │                  │local signature verify + forward by zone_hint
              │ AdmissionRequest   │issue token         ▼
              ▼ (RequestReply)     │              baseapp (create session entity → SessionUp)
            baseappmgr (gate/displacement/placement/pending nonce) ──► #12 directory (visibility anchor)
```

| Party | Duties | Explicitly not doing |
|---|---|---|
| login-app | Account authentication (reads the account domain), admission-request proxying, token issuance, contract pack pointer delivery, (P3) queueing notification forwarding | Holds no directory/load table (baseappmgr's authority), does not verify game connections, does not touch gateway/baseapp, does no file serving |
| baseappmgr | Admission gate (watermark), displacement pre-ruling (#12 §3 critical section), placement (lightest baseapp + gateway pair), pending nonce redemption, (P3) queueing queue | Does no authentication (credentials never leave login-app), signs no tokens |
| gateway-app | Token local signature verification, passthrough routing by zone_hint (sdk-contract §10.6) | Does not touch the account store (§5.9 :313), makes no admission rulings (the things it would ask baseappmgr were already completed in phase one) |
| baseapp | Creates session entities, reports SessionUp (#12 anchor) | Unaware of the login chain (the token is transparent to baseapp — it only receives the placement directive issued by baseappmgr) |

**Why login does not go through gateway (counter-plan record)**: the ops benefit of a single entry address &lt; the cost — gateway would have to multiplex two forms at once ("pre-handshake plaintext routing" and "encrypted four-channel sessions"; the L2 stack sniffs first then splits, complicating the state machine); account credentials entering gateway = enlarged sensitive surface; login storms and game traffic would live or die together. BW/KBE both use an independent loginapp — a twenty-year-consistent precedent. The client's two addresses (login/gateway) come from config or are fetched by the launcher (a P3 bootstrap endpoint could be added; not expanded here).

## 2. Login Connection Protocol (Phase One)

**Handshake (same family as §5.9, domain-separated)**:

```text
C → L  LoginHello      { nonce_c, pub_c(X25519), aead[] }          // plaintext (public keys are plaintext anyway, same form as §5.9 ClientHello)
L → C  LoginHelloAck   { nonce_s, pub_s(X25519), aead_selected, salt }
Both    login_key = HKDF-SHA256(X25519(·), salt = nonce_c‖nonce_s, info = "apollo-login-v1"‖login_conn_id)
C → L  LoginAuth       { credential(AEAD), auth_mode, client_hash, platform }   // credentials inside ciphertext
L → C  LoginResponse   { result, account_id?, login_token?, gateway_addr?, zone_hint?, client_hash_ok, bundle_url? }
       → connection closed (the login connection is one-shot, no resumption no resume — on failure redo the whole handshake)
```

- **Two auth_mode types**: `password` (username + password → accounts credential check) and `channel` (third-party channel token → outbound verification §5.10 → binding lookup #14 §5) — one type, one verification; P1 implements only password.
- **Credentials never land in logs** (outside the whitelisted surface of logging's tiered bypass); LoginAuth failure counts feed the login-app-side lockout policy (the retained form of the legacy :48-121 maxLoginAttempts/lockout prototype — account-level, applied before PBKDF2).
- **Error-code family** goes into errors.xml (with a code batch; contract files frozen untouched): bad_credentials / account_banned / server_busy(+retry_after) / queue (P3) / client_hash_mismatch / channel_rejected (#14).
- Frame layer: the same L1 frame format (magic/seq/CRC32C) + CryptoFilter (§5.5 slot; position after compression unchanged); login messages belong to the **client domain** (client-visible surface; id segmentation governed by §11.3).

## 3. login_token (Structure / Issuance / Verification / Redemption)

| Item | Ruling |
|---|---|
| Structure | `base64(payload ‖ HMAC-SHA256(K_login, payload))`; payload = {account_id, issue_tick, exp_tick, nonce(128bit), gateway_id, zone_hint, world_assignment initial value, purpose="game_entry"} |
| TTL | Short (60s default recommendation — the window from "login response to ClientHello", calibrated in the P3 capacity batch); exp rejection code token_expired → the client redoes phase one (no resumption) |
| Issuance | Exclusive to login-app (K_login privately held); the purpose field prevents cross-purpose replay (future extensions such as purpose="gm_entry" are not interchangeable with it) |
| Verification | gateway local (the K_login verification key distributed via G-1 fleet config — static configuration state, not runtime negotiation); verification = three checks of integrity + exp + purpose, **does not touch login-app/the account store** (fulfilling §5.9 :313 "game processes only verify signatures" — gateway and baseapp are both game processes) |
| One-time | **Redemption point = the baseappmgr admission critical section**: at AdmissionRequest the pending table records the nonce (TTL = token TTL + margin) → at SessionUp the pending entry converts to a formal #12 entry and is redeemed → a second use of the same nonce = no such pending record / entry already exists → rejection + violation_score (§4.3 :128 same bucket). **Zero new state surface**: the pending table is the login-side companion table of the #12 directory (baseappmgr memory, TTL self-cleaning, timed by the baseappmgr timer family — same component dependency as #12 §4 window timing) |
| Key | Symmetric HMAC-SHA256 (internal fleet trust domain; gateway holding the verification key = inside gateway's trust boundary, same trust level as CryptoFilter); rotation = fleet config versioning (P3, K_login_v[N] dual-active window) |
| Algorithm compliance | HMAC-SHA256 is within the permitted surface of §5.9's algorithm tier table (SHA-256 digest row); nonce/randomness uses only RAND_bytes (§5.9 usage discipline — the legacy :284-301 self-made xorshift PRNG is the counter-example; migration surface §10) |

**Division of labor versus the other two credentials** (easily confused; concept-glossary §2.5): login_token = the account-domain one-time entry ticket (this design); session_key = the connection-domain frame encryption key (§5.9 handshake negotiation, new per connection); resume token = the same-session disconnect-recovery credential (§3 L2; TTL same-sourced with the disconnect window per #12 §4). The three layers are not interchangeable and do not derive from each other (a supplement to the credential surface of the §5.9 :317 key hierarchy).

## 4. Account Domain

- **owner = DB** (storage.xml statement set, batch 2 db-app — the attribute-sync §8.2 statement surface): `accounts` (account_id PK / credential_salt / credential_hash / state / banned_until / created_at / last_login_wall_ms — **last-online time / last baseapp are the archive columns previewed at #12 :114**; column promotion belongs to the batch 2 statement set) + `third_party_bindings` (channel / open_id / account_id / bound_at, UNIQUE(channel, open_id) — #14 §5 consumes the same table).
- **Credential hashing = PBKDF2-HMAC-SHA256** (OpenSSL `PKCS5_PBKDF2_HMAC` built-in — the single crypto source stays intact, no new dependency introduced; iteration parameters in config, calibrated in P3); salt per-account 128bit RAND_bytes. The legacy plaintext comparison (:27/:62) is the P1 counter-example.
- **Authentication path**: P2 single process = login-app embedded statement execution (same-process db face); P3 = RequestReply → db path (low-frequency control plane, §5.7 invoke_mode compliant — authentication never enters any tick hot path).
- **Risk-control consumption**: violation_score abuse disconnects and writes an account-level risk-control record (§4.3 :128) → banned_until is set by GM ban commands (per the existing semantics, scripting-lua §7.5 kick/ban execution points to the login-app gate); at admission baseappmgr or login-app checks state/banned_until to refuse login (checkpoint fixed at login-app — baseappmgr does not touch the account store).
- **P1 degradation**: in-process test account table (legalization of the legacy :22-41 shape: in-memory map + SHA-256 salt at the lowest tier; PBKDF2 joins in P2); third-party channel mode in P3 with #14.

## 5. Server Selection / Queueing / Admission (baseappmgr side)

**AdmissionRequest processing order (baseappmgr critical section, serial)**:

1. **Load gate**: fleet watermark (net §7 :395 admission gate, metrics same source as G-5) — overload → busy + retry_after (P1-P2 terminal form) / enqueue (P3).
2. **Displacement pre-ruling**: on hitting an existing #12 directory entry (Online/Suspended/Leaving) → in the same critical section the old entry is set Removed + asynchronous kick delivery (SessionKicked + gateway close(kicked) — the full #12 §3 set; this design only triggers, it does not redefine).
3. **Placement**: lightest baseapp (minAppLoad same form + watermark hard constraint) + gateway pair (pick one from the gateway cluster — baseappmgr's global view) → world_assignment initial value (P1 = configured default world; P3 = player's last online / instance re-entry logic, a business hook slot).
4. **Pending record**: nonce + placement + TTL → return to login-app.

- **Why admission happens at login time rather than ClientHello time**: BW same form (loginapp asks the mgr for (addr,load) — :1117 routing); displacement ruling naturally happens at the earliest point where "new login" semantics are decidable (#12 §3 "new login hits an entry" is exactly this moment); ruling again at ClientHello = one extra round of failed retries for the client. **The cost is declared explicitly**: if the player never connects after issuance (abandons the token) → pending TTL self-cleans, no ghosts (same as #12's "no ghosts left behind" discipline).
- **Queueing (P3)**: a baseappmgr-side admission queue component — capacity-watermark-driven enqueue/dequeue, queue_position events delivered via login-app (while the login connection is held), and dequeue enters critical-section steps 2-4. Neither precedent has it built in (BW LoginConditions is rejection-style) — apollo builds it as an optional component per the CN-MMO requirement, off by default.
- **Declaration of critical-section sharing with #12**: admission rulings and directory rulings sit at the same baseappmgr serialization point (a direct beneficiary of #12 :9's single-point-serialization argument) — displacement, admission, and registration: three matters, one critical section, no cross-process locks.

## 6. Contract Pack Delivery Timing (Pointer-Based)

- LoginResponse carries `{client_hash, bundle_url}`; the client compares against its local manifest (§11.6 three-piece set) → on mismatch fetches via bundle_url (CI `contract_pack` already produces `dist/client/<client_hash>/` — the sdk-contract §12.2 landing note) → per-file SHA-256 gate at load time (existing).
- **login-app does no file serving**: byte distribution belongs to the deployment face (CDN/static directory); the engine ships only pointers — KBE clientsdk_downloader's "engine as CDN" is not adopted (apollo has a CI packaging pipeline; an engine process carrying bandwidth and caching duties is a form mismatch). P1 degradation = configured static URL; client_hash_ok = false and no URL = version-too-old upgrade prompt (forced-update slot, launcher semantics, not expanded here).
- The schema_hash handshake stays on the **game connection** as before (sdk-contract §6 N/N-1 window) — the login connection only compares client_hash (coarse-grained entry check; the fine-grained load-time gate already exists).

## 7. Whole-Chain Sequences (Four Scenarios)

1. **Normal**: LoginHello → LoginAuth → (login-app: authentication) → AdmissionRequest → (baseappmgr: gate/placement/pending) → LoginResponse{token, gateway, zone_hint, bundle} → disconnect → ClientHello{token} (gateway verifies and forwards) → baseapp creates entity → SessionUp (baseappmgr redeems the pending entry, directory visible) → §5.9 session established, entering the world.
2. **Displacement**: a new login on the same account → AdmissionRequest hits a directory entry → in the critical section the old entry is set Removed + kick delivery (old baseapp SessionKicked / old gateway close(kicked)) → the new chain proceeds as usual (the old end gets the kicked reason code per #12 §8). The old end's resume race is rejected by anchor_epoch (#12 §3-3).
3. **Disconnect reconnect**: the game connection dies → directory Suspended (#12 §4 window) → the client **goes straight to gateway with ClientHello + resume token** (L2 §3 — not through phase one, no new login_token issued; resume ≠ new login, two matters kept separate per #12 :11); window full without recovery → the full new-login chain (phase one from scratch).
4. **Overload**: AdmissionRequest hits the gate → busy + retry_after → LoginResponse rejected → client backs off and retries (under P3 queueing it holds the login connection to receive queue_position).

## 8. Message Surface

| Family | Domain | Messages | invoke_mode/channel | Notes |
|---|---|---|---|---|
| Login handshake | client | LoginHello / LoginHelloAck / LoginAuth / LoginResponse | login connection (L1 frame + CryptoFilter) | §2 full sequence; enters the messages.xml client segment with a code batch |
| Queueing notification (P3) | client | QueueNotice{position, eta} | login connection | forwarded baseappmgr → login-app |
| Admission | internal | AdmissionRequest / AdmissionReply | RequestReply (low-frequency control plane compliant; §5.7 :217 timeout does not revive — login-app retransmission is idempotent via nonce) | login-app ↔ baseappmgr |
| Admission cancellation | internal | AdmissionCancel | OneWay | login connection dropped and token not issued → withdraw the pending entry (anti-squatting; TTL backstop) |
| (Existing) #12 event family | internal | SessionUp/Down/Moved/Kicked + Snapshot reconciliation | see #12 §8 | SessionUp is the redemption point (§3) |

- Two new internal-domain messages enter the sdk-contract §11 segmentation (900+, with a code batch; coexisting in the same domain as #12's five-message family); envelope = InternalMessageEnvelope (net §7).
- **Client-domain additions = the four login-family messages** — #12's earlier "zero client-domain additions" was its directory-surface semantics; the login family is #13's legitimate addition (the client-visible surface already belongs to the client domain), and the domain discipline (cross-domain bans / segmentation §11.3) stands.

## 9. Intersections with Other Designs

| Related design | Placement |
|---|---|
| net-abstraction | §5.9's login_token attribution line fulfilled as §3's full specification (structure/redemption/key); the login handshake = a §5.9 same-family domain-separated instance; §5.7 Admission RPC = RequestReply control plane; §7 gate/lightest allocation = §5 processing-order steps 1/3; §4.3 violation_score = one of the token-replay count sources |
| session-and-online-directory | §2 registration anchor = this design's chain endpoint; §3 displacement = admission critical-section step 2 (the trigger is here, the ruling semantics there); §4 window/resume division of labor (scenario 3); the pending nonce table = the directory's companion table (§3); :114 account archive columns = §4 accounts table |
| sdk-contract | §11.3 client-domain login-family four messages + internal-domain two messages (with a code batch); §11.6/§12.2 contract pack pointer scheme; §6 schema_hash handshake stays on the game connection |
| attribute-sync | §8.2 storage.xml accounts/third_party_bindings statements (batch 2); §8.3 migration discipline applies to account-table DDL |
| scripting-lua | §7.5 ban GM commands → banned_until set (closes §4's risk-control consumption loop); §8 asynchronous handoff does not apply to authentication (low-frequency control plane with synchronous RequestReply is compliant) |
| inbound-interfaces (#14) | third_party_bindings: one table, two consumers; the channel login mode = the entry side of #14's binding domain; channel_rejected error code in the same family |
| capacity-and-benchmark | login surges (reconnect storms) = a login-app capacity item; token TTL/pending table size/queue depth enter the P3 calibration checklist |
| clock-and-time | issue/exp_tick = tick numbers; pending TTL = a baseappmgr timer deadline (#12 §4 same component dependency) |
| concept-glossary | the login-chain entry + §2.5 three-credential disambiguation (bundled with this batch) |

## 10. Legacy Migration and Phasing

**Legacy disposition (item by item from the §25 read; migration belongs to a code batch, source frozen untouched)**:

| Legacy piece | Disposition | Target form |
|---|---|---|
| Authenticator (:22-121 hardcoded/lockout prototype) | Keep the skeleton | Componentize the account domain (§4: PBKDF2 + accounts table + P1 degradation table); lockout policy retained |
| GatewayAllocator (:133-179 least-connections gateway choice) | **Duty merges into baseappmgr** | login-app deletes this piece — gateway choice/baseapp choice belong to baseappmgr placement (§5-3) |
| SessionManager (:185-301 ticket double hashing/sliding renewal) | **Delete** | Replaced by the self-contained token (§3) + baseappmgr pending redemption — no server-side session table |
| RepSocket :9001 + legacy protocol codecs | Migration surface | L1 frame format + client-domain login family (§2) — same batch as the net M1 kernel |
| preparePlayerOnline triple call (:476-542, incl. :536-541 fake success) | **Delete** | Replaced by baseappmgr admission + baseapp SessionUp (the phase-two chain); the fake-success path has no counterpart |
| gateway session_admission_service RPC round-trip (:33) | Migration surface | Local signature verification (§3) — gateway switches to holding the K_login verification key |
| main.cpp command-line config (:20-47) | Migration surface | Fleet config (G-1 configuration state) with the assembly batch (architecture-review §17/§21 semantics) |

**Phasing**:

- **P1 (single-process demo)**: in-memory test accounts (§4 P1 degradation) + in-process admission (baseappmgr duties called directly in the same process, zero RPC) + token HMAC full specification (key in config state) + the login message family enters the contract — **zero new processes**.
- **P2 (Compact / single process complete)**: accounts table lands in storage.xml (batch 2 dependency) + PBKDF2 + contract pack pointers + displacement/window closed loop (interfacing with #12's P2 degenerate form).
- **P3 (multi-process full)**: login-app/baseappmgr split processes + Admission RPC + queueing component + channel login (#14 same batch) + key rotation + surge capacity calibration. **Mutual acceptance with #15 Bots** (bots script their logins through this chain — fulfilling the §26-④ handshake dependency; displacement storms/disconnect-reconnect stress tests = same source as #12 §9's acceptance suggestions).

---

*Baseline: apollo main @ edc94cfa (documentation state; source frozen untouched). Legacy read = architecture-review §25 (apps/login-app all 884 lines, gateway ingress admission surface); precedent citations = No. 36 :55/:66 (BW/KBE loginapp), net-abstraction §7 :389/:1117 routing and LoginConditions, deep-dive :100 (clientsdk_downloader); #12 interlock surface = session-and-online-directory :9/:40-48/:55-63/:71/:77-85/:87-91/:114/:119-127 actually read. Design-batch inputs = architecture-review §25 six questions (this draft's six answers: topology ①/redemption ②/authentication source ③/admission division ④/SDK ⑤/migration ⑥) + §26-④ handshake dependency. TTL/iteration parameters/surge capacity are initial recommendations, calibrated in the P3 capacity batch.*
