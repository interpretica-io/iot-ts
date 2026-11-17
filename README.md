# iot-ts

A Test Environment suite that exercises the major **IoT technologies**
against the Test Agent it runs on — one group per technology, each
emitting a measurement chart and, where it applies, a security audit.

| Group | Library | What it does |
|---|---|---|
| `mqtt` | tsf-mqtt | publish/subscribe round-trip latency, and a broker audit (anonymous/wildcard/plaintext) |
| `coap` | tsf-coap | `/.well-known/core` discovery, GET, and a CoAP audit |
| `lorawan` | tsf-lorawan | build/parse LoRaWAN frames, verify MIC and payload, and a frame-counter-reuse audit |
| `ieee802154` | tsf-802154 | build/parse 802.15.4 frames (the layer under Zigbee/Thread/Matter) with AES-CCM*, and a counter-reuse audit |
| `mdns` | tsf-mdns | mDNS/DNS-SD service discovery over loopback |
| `upnp` | tsf-upnp | UPnP/SSDP discovery, device description, and an audit |
| `ble` | tsf-bluetooth | adapter enumeration, a device-discovery scan, and an audit |
| `wifi` | tsf-wifi | WiFi station configuration through the `/agent/wifi` subtree |
| `gpio` | tsf-gpio | GPIO/I2C/SPI bus enumeration and access |
| `sensors` | tsf-sensors | hardware monitoring: temperatures, fans, voltages against their limits |
| `watchdog` | tsf-watchdog | the hardware watchdog identity and timeout |
| `ics` | tsf-ics | Modbus register/coil reads and an industrial-control audit |
| `cwmp` | tsf-cwmp | the TR-069 (CWMP) message codec, round-tripped with no ACS |
| `security` | tsf-fuzz, tsf-time | the cross-cutting hardening layer: fuzzing a frame parser and a fake-clock expiry probe |

## The shape of these tests

Two kinds of test live here. The **codec and fuzz** groups (lorawan,
ieee802154, cwmp, security) are self-contained — they build, parse and
abuse frames in-process, so they run anywhere with no device. The
**protocol, discovery, bus and radio** groups talk to something real (a
broker, a CoAP/Modbus/UPnP device, an adapter, a `/dev` node); on a host
without it they **SKIP cleanly** rather than fail.

Every audit **plants a known-bad control** next to it: a reused frame
counter, an anonymous broker, an unauthenticated control surface. The
assertion is that the audit *reports* the planted defect, so an audit
that reports nothing is the failure.

Every test that measures a number emits a native TE **MI line-graph**
(throughput, latency, device count, decode time, temperature…), so the
HTML log and Bublik draw it.

## Running it

The suite expects TE as a sibling directory:

```bash
git clone https://github.com/interpretica-io/test-environment ../test-environment
./scripts/run.sh docker guess --cfg=localhost
```

Results: `cat log.txt`, or `./scripts/html-log.sh` for the report (open
it and confirm the line-graphs are there).

On the `localhost` configuration the device under test is the build
container; the codec/fuzz groups run and the hardware/daemon groups skip.
To exercise a real device, name a host in `conf/rcf.conf`, point the
relevant `TSF_*` environment variables at it (e.g. `TSF_MQTT_HOST`,
`TSF_ICS_HOST`), and for WiFi set `TA_WIFI_A_HOST` at build time.

### The libraries have to be reachable

`conf/external.yml` names every `tsf-*` repository and `dispatcher.sh
--external` clones them. While a library is being developed, point its
`url` at a local checkout and set `ref` to the branch you are on:

```yaml
  - name: tsf_mqtt
    url: /home/you/src/tsf-mqtt
    ref: main
```

The commit actually built is recorded in `conf/builder.conf.lock`;
`dispatcher.sh --update-external` moves it on.

### What the agent needs

This suite has a **real Test Agent** (`Agt_A` on the engine host), with
`ta_rpcprovider` attached — the agent-side modules (coap, mdns, ics,
gpio, sensors, watchdog, upnp) run their RPCs there, and the engine-side
ones (mqtt, bt) drive their tools through its job factory. The native
libraries those agent modules link (libcoap v3, avahi, libmodbus,
libgpiod, lm-sensors, libupnp) are installed by `build.sh --extra-deps`
in `conf/builder.conf`.

Tools and devices are looked for before use; a group that cannot find
what it needs skips rather than fails. The `security` fuzz campaign
carries the `LONG_TERM` requirement, which `conf/run.conf.localhost`
excludes, so an ordinary run does not sit through it; to include it:

```bash
./scripts/run.sh docker guess --cfg=localhost --tester-req=LONG_TERM
```
