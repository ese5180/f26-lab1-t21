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
