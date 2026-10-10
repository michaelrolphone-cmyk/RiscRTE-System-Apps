# Foreground sharing client

`PortableFileSharing.h` is an app-local C interface over the existing Digest,
session and storage controller. Each start is explicit, finite and authenticated.
The app selects unencrypted local HTTP before calling start. No listener opens
at load, and no credentials persist. The network connection is separately owned
by the caller; this client neither starts nor stops Wi-Fi.

The client acquires only the explicitly configured `storage.app-data.export`,
`network.tcp.listener`, and `crypto.entropy` grants. It never chooses native
filesystem paths, namespaces or export authority. Browser-held export grants
must be closed before starting: Runtime allows one export invocation per app.

One normal tick performs at most one capability operation. Entropy BUSY and
UNAVAILABLE leave preparation active and retry only on later foreground ticks.
A single 32-byte fill supplies independent 16-byte password and nonce material.
The password uses sixteen symbols from a 32-symbol alphabet (80 random bits);
the nonce uses the other sixteen bytes. The entropy grant closes before the
listener opens. Password display is available only once listening succeeds and
is wiped immediately on stop, failure, expiry or owner loss. The Digest session
stores a derived value, not its input password.

An active session inhibits automatic idle even without a connected client. A
manual transition first closes the client socket and listener, then releases
grants. The bounded synchronous close helper calls only nonblocking operations;
there are at most eight internal steps. If any close/release loses custody, it
returns false without any further capability call, free, display or diagnostic.
The app must preserve its invocation and workspace in that case. A clean stopped
client can be restarted with fresh credentials or explicitly destroyed.

Normal and ASan/UBSan tests cover actual Digest/HTTP read interoperability using
an independent OpenSSL response, transient entropy, unavailable capabilities,
allocation failure, stop/expiry during preparation, connected cancellation,
close/release refusal, owner loss, clock rollback and fresh restart. The generic
protocol/controller Xtensa object probe is not a complete product build or a
hardware/network qualification.

## Source preservation

The protocol, two ordinary providers and their SDK headers were copied without
changes from public System commit `9771cc2fa42b518b5c22c33742468afcb98bc3dd` onto
the current UI tree `b6438da42894a5b3c3ab63b824d55ab0e717cc2b`. The Browser port,
sharing frontend, owned async Wi-Fi connection and BLE setup are separate
integration steps. No existing adapter/controller file is replaced by that copy.
