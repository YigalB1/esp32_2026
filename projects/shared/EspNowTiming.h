// EspNowTiming.h
//
// SHARED FILE - copy this exact file into every project that SENDS
// EspNowMessage heartbeats (train controllers, traffic lights, etc.).
// Lives alongside EspNowProtocol.h under projects/shared/.
//
// This only needs to hold values that must be consistent *across multiple
// sender devices*. Liveness thresholds ("maybe" / "not connected") are
// dashboard-side display policy, computed purely from the dashboard's own
// wall-clock gap since each device's last message - they don't need to be
// shared here, and this header is never read by the dashboard (Python).
//
// ACTION REQUIRED: copy this file into every other sender project's
// shared include path alongside EspNowProtocol.h (train_ctrl_c3, ramzor,
// any future device) so they all heartbeat at the same cadence.

#pragma once

// How often each device should send a REASON_HEARTBEAT message when
// otherwise idle (no real state change to report). Consumers (the
// dashboard) use "any message from this device, any reason" as the
// liveness signal - see EspNowProtocol.h's REASON_HEARTBEAT.
static const unsigned long HEARTBEAT_INTERVAL_MS = 20000; // 20s
