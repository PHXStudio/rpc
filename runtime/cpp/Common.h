#ifndef __COMMON_H__
#define __COMMON_H__

/** @page arpc Asynchronous RPC overview.

The arpc system (asynchronous remote procedure calling) is CORBA-like: it maps networking to
object-style calls and keeps protocol code small.

@section aprc_ref References
- Google Protocol Buffers
- CORBA

These are mature, but for games they are often too heavy, not fast enough, and hard to extend
with a custom IDL.

@section desc Design
arpc targets a small RPC core: marshal/unmarshal and dispatch only; everything else is out of scope.

@par Features
- Message parsing is mostly generated; fewer hand-written parsers.
- Schema changes stay localized.
- RPC is decoupled from transport and IO models.
- The IDL compiler can emit test helpers, loggers, etc.

All calls are asynchronous: only input parameters, no output parameters or return values.
Treat peers as async request/event handlers.

@par Components
An IDL compiler plus a small runtime library.
@image html arpc.png
@see Sepcification ProtocolWriter ProtocolReader

@subsection aprc_stub Service stub
The compiler emits a stub that marshals a member call into a ProtocolWriter. Subclass
ProtocolWriter to send bytes over your transport.

@subsection proxy Service proxy
The compiler emits a proxy for the callee side. Call the global dispatch with a ProtocolReader
and a proxy instance to demarshal into member calls. Subclass the proxy and implement handlers.
@note The proxy only demarshals; locating the handler object is up to you.

@section safety Security
If the sender is untrusted (e.g. a game client), tighten checks; receiver-side checks are generated.

@par Dynamic-sized fields
Avoid dynamic-sized methods when possible. If you use arrays or strings, set maximum lengths.

*/

#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>
#include <ostream>

/** IDL enumeration values use a single wire type: they are read and written as uint8_t
    in binary payloads (same width as C# byte). The C++ enum type may be wider; generated
    code casts at the protocol boundary. */

#endif//__COMMON_H__
