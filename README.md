# ESE5180: Lab 1 Wireless Comms

**Group Number:** 21

| Team Member Name | Email Address       |
|------------------|---------------------|
| Vihaan Ravishankar | [Penn email]      |
| [Name 2]         | [Email 2]           |

**GitHub Repository URL:** 

## 1

### 1.1 LoRa Range Challenge

Code is in `1_1_lora_range/` as `1_1_send.c` and `1_1_receive.c`.

Radio config on both boards:

- 433.92 MHz
- SF12, 125 kHz bandwidth, coding rate 4/5, 8 symbol preamble
- payload CRC disabled
- `tx_power` left at -10

We started from the appendix framework and only changed two things, the
spreading factor and the CRC. SF10 to SF12 is worth about 5 dB of receiver
sensitivity, which is the biggest lever left once the transmit power is fixed
at -10. The problem is airtime. At SF12 with the CRC on, a 12 byte packet takes
1156 ms, which is over the 1 second burst limit, and the framework's own check
refuses to transmit. Dropping the CRC takes it to 992 ms and it fits with about
9 ms to spare.

Airtime comes out to 992 ms, so the quiet period is 29760 ms between packets.

The coding rate stayed at 4/5 and the preamble at 8 symbols for the same
reason. A symbol costs 32.768 ms at SF12 and 125 kHz, so moving to 4/8 for
stronger error correction adds about 196 ms and puts us back over a second. The
1 second burst limit is really what picks the configuration for us, not
sensitivity.

The payload is `ese5180t21-0` and the last character increments on every send,
so the receiver log shows which packets got through and which ones dropped as
we walked out.

Longest distance packet:

```
RSSI: TODO dBm
SNR: TODO dB
Distance: TODO
```

In firmware the range came almost entirely from the spreading factor, since the
transmit power is fixed and the burst limit leaves no airtime to spend on error
correction. On the hardware side, TODO (antenna, where we stood, line of sight).

The cost is throughput and energy. Twelve bytes take nearly a full second of
airtime, and with the 30x quiet period we get one packet every 31 seconds,
which works out to about 3 bits per second. The radio is also transmitting for
that entire second, so each packet is expensive for a battery. Turning the CRC
off makes it worse in a different way, because the receiver now hands up
whatever it decodes and we have to read the payload and the counter ourselves
to tell a good packet from a corrupted one.

A real product would not sit at SF12. It would adapt, starting at a low
spreading factor and only climbing when packets start failing, and it would
leave the CRC on so the radio drops bad frames before the application sees
them. It would also need addressing. Every board in the class runs the same
sync word with no device ID in the payload, which is why we were all receiving
each other's packets. LoRaWAN handles that with a device address and a message
integrity code, so a gateway ignores anything that is not from one of its own
devices.

### 1.2 Trading Range for Bandwidth

Code is in `1_2_lora_bandwidth/` as `1_2_send.c` and `1_2_receive.c`.

Radio config on both boards:

- 433.92 MHz
- SF5, 500 kHz bandwidth, coding rate 4/5, 12 symbol preamble
- payload CRC enabled
- `tx_power` left at -10

Same payload as 1.1, `ese5180t21-0` with the counter, so only the radio
settings changed.

Fastest transmission speed:

```
Airtime per 12 byte packet: 3.6 ms (firmware logs "Packet airtime: 4 ms", it rounds up)
On-air data rate: 96 bits / 3.6 ms = about 26.7 kbps
1.1 for comparison: 991 ms per packet, about 97 bps on air
```

That is about 275 times faster on air than the range configuration.

Parameters we changed from 1.1:

- **Spreading factor, SF12 to SF5.** Every step down halves the symbol time,
  so this is the biggest change. SF5 is the lowest the STM32WL radio supports.
- **Bandwidth, 125 kHz to 500 kHz.** 4x the bandwidth gives 4x shorter
  symbols. 500 kHz is the widest the SX126x radio core supports (Zephyr's
  driver only takes 125, 250 and 500 for this chip). At 433.92 MHz the signal
  covers 433.67 to 434.17 MHz, still inside the 433.05 to 434.79 MHz band.
- **Preamble, 8 to 12 symbols.** The radio requires at least 12 at SF5 and
  SF6, and the driver forces it anyway. At SF5 and 500 kHz a symbol is only
  64 us, so this costs about 0.26 ms.
- **CRC back on.** At SF12 the CRC cost 164 ms and pushed us over the 1 second
  limit. Here it costs 0.32 ms, so there's no reason to leave it off. The
  radio drops corrupted frames again, so we don't have to check them ourselves.
- **Coding rate stayed at 4/5**, which is the least redundant setting and so
  already the fastest.

Power goes down because the radio transmits for much less time. The transmit
current at -10 dBm is roughly the same in both configs, but the radio is on
for 3.6 ms instead of 991 ms, so each packet takes about 275 times less
transmit energy. The MCU is idle in `k_sleep` for the rest of the cycle.

The quiet period rule now sets the throughput, not the radio. 30x the airtime
is only about 120 ms, so the 10 second minimum applies. We send one packet
every ~10 s, which works out to about 9.6 bps overall, compared to about 3 bps
in 1.1. The on-air rate is about 2,800 times higher than that, so the radio
sits idle for more than 99.9% of each cycle.

The cost is range. SF5 at 500 kHz is roughly 23 dB less sensitive than SF12 at
125 kHz (about 17 dB from the spreading factor and 6 dB from the wider
bandwidth). At -10 dBm through the mismatched antenna, that means a short range.

**Spreading factor.** In LoRa each symbol is a chirp that sweeps the whole
bandwidth, and it carries SF bits. The spreading factor sets how long the
chirp is: 2^SF / BW seconds. Each step up doubles the symbol time and adds
only one bit per symbol, so the raw bit rate roughly halves. In return, each
symbol carries more energy and the receiver can pull it out from further below
the noise floor (about 2.5 dB more sensitivity per step). So the spreading
factor affects bit rate, airtime, energy per packet, receiver sensitivity and
range, and how long the channel is occupied, which affects collisions with
other devices. The tradeoff is range and robustness against speed and battery.

**Applications for the low bandwidth option:**

- *Agricultural soil moisture or weather sensors.* A few bytes every 15 minutes
  across a large field or farm where there's no infrastructure. Range means one
  gateway can cover the whole property, and nobody minds that a reading takes
  a second to send.
- *Utility meters (water and gas).* These are often in basements or pits
  behind concrete, so they need the extra link budget. A meter reading is tiny
  and only needs to go out once or twice a day.
- *Asset or livestock trackers in remote areas.* A GPS fix every hour across
  miles of open land. Fewer gateways cover more ground, which matters more
  than throughput.

In all of these the payload is small and infrequent, so the bit rate barely
matters. Range directly reduces how many gateways you need to deploy, and the
long airtime only costs energy a few times a day.
