# Companion protocol v1

The shared [platform specification](../platform/README.md) defines binary framing and typed service methods. Definitions live under `platform/spec/`; Core, Desktop and the simulator use identical codecs.

Compatibility checks include the development contract revision, build ID and required capabilities. USB and BLE have no textual application-command fallback. BLE requires encrypted authenticated bonding, with pairing approvals restricted to a retained local USB channel.

The UUID family remains `e7f1000{1,2,3}-7a1e-4b5c-9d2f-46544c494e54` for service, RX and TX. Acknowledged 20-byte GATT fragments carry the same complete binary frames as USB.
