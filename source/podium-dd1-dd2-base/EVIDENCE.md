# Podium DD1 and DD2 Evidence Record

## Scope

This file records behavior recovered from the original base firmware.

The reference program is `podium-dd1-dd2-base.hex` in the Ghidra project `podium-dd1-dd2-base`.
Ghidra identifies the program as dsPIC33E little-endian 24-bit code with 1,008 functions.

An address with the `rom:` prefix refers to an instruction in that program.
An address with the `ram:` prefix refers to a data address used by that program.

The device pack identifies peripheral register names and bit fields.
The binary establishes each register value and each control flow decision.
Archive source is not evidence for any behavior in this project.

## Evidence states

| State | Meaning |
| --- | --- |
| Verified | The binary or device pack directly supports the claim. |
| Derived | Several verified operations support the claim. |
| Unknown | The available binary trace does not yet establish the meaning. |

## Normal attached-wheel link

| Claim | State | Evidence |
| --- | --- | --- |
| The normal link uses UART3. | Verified | `rom:04e508` writes the UART registers at `ram:0250` through `ram:0258`. The device pack maps these addresses to UART3. |
| RF2 is an output and RF8 is an input. | Verified | `rom:04e53e` clears TRISF bit 2. `rom:04e540` sets TRISF bit 8. The device pack maps TRISF to `ram:0e50`. |
| RF2 and RF8 use pull-down resistors. | Verified | `rom:04e542` and `rom:04e544` set CNPDF bits 8 and 2. The device pack maps CNPDF to `ram:0e5c`. |
| The receiver and transmitter signals are inverted. | Verified | `rom:04e55e` sets U3MODE bit 4. `rom:04e560` sets U3STA bit 14. |
| The normal link uses high-speed baud generation. | Verified | `rom:04e556` sets U3MODE bit 3. |
| The normal link sets U3BRG to 2. | Verified | `rom:04e562` loads 2. `rom:04e564` writes that value to U3BRG. |
| DMA5 transmits bytes from RAM to UART3. | Verified | `rom:04e430` through `rom:04e44a` set the transfer direction, request 0x53, and U3TXREG address 0x254. |
| DMA5 sends 72 bytes in one-shot mode. | Verified | `rom:04e440` sets DMA5 mode bit 0. `rom:04e44c` writes a count of 0x47. |
| DMA5 uses interrupt priority 5. | Verified | `rom:04e456` through `rom:04e45c` write priority value 5. |
| DMA6 receives bytes from UART3 to RAM. | Verified | `rom:04e4d8` through `rom:04e4f2` set the transfer direction, request 0x52, and U3RXREG address 0x256. |
| DMA6 receives 68 bytes in one-shot mode. | Verified | `rom:04e4e8` sets DMA6 mode bit 0. `rom:04e4f4` writes a count of 0x43. |
| DMA6 uses interrupt priority 4. | Verified | `rom:04e4fe` through `rom:04e504` write priority value 4. |
| Each transmitted frame has four leading and four trailing bytes with value 0xF0. | Verified | `rom:04e3d8` writes 0xF0 at offsets 0 through 3 and 68 through 71. |
| A received frame can start at offsets 0 through 4. | Verified | `rom:04e3fc` searches five positions for value 0x7B. |

The device-pack register map is in `p33EP512MU810.gld`.
UART3 appears at lines 1028 through 1039.
DMA5 and DMA6 appear at lines 2574 through 2615.
TRISF and CNPDF appear at lines 3172 through 3192.

## Transport frame

| Byte | Size | Meaning | State | Evidence |
| --- | ---: | --- | --- | --- |
| 0 | 1 | Start delimiter 0x7B | Verified | `rom:04c0d2` through `rom:04c0d4` |
| 1 | 1 | Command and fragment flags | Verified | `rom:04c0da`, `rom:04c102` through `rom:04c104`, and `rom:04c114` through `rom:04c128` |
| 2 | 1 | Packet counter | Verified | `rom:04c0dc` through `rom:04c0e0` and `rom:04c296` through `rom:04c298` |
| 3 | 1 | Payload length | Verified | `rom:04c0e2` |
| 4 | 57 | Payload capacity | Verified | `rom:04c108` compares the message size with 0x39. |
| 61 | 2 | CRC-16, low byte first | Verified | `rom:04c0f0` through `rom:04c0f8` |
| 63 | 1 | End delimiter 0x7D | Verified | `rom:04c0d6` through `rom:04c0d8` |

The encoder clears all 64 frame bytes before it writes fields.
This operation appears at `rom:04c0cc` through `rom:04c0ce`.

The CRC starts at zero and covers bytes 1 through 60.
The loop length appears at `rom:04c090`.
The update operation appears at `rom:04c070` through `rom:04c08e`.

The transport test vector uses command 3, packet counter 0, length 3, and payload `01 02 03`.
The remaining payload bytes are zero.
The recovered CRC operation produces 0x39A9.

The receiver checks both delimiters before it checks the CRC.
These checks appear at `rom:04c262` through `rom:04c280`.

## Message transfer

The low nibble of byte 1 selects the command.
The receiver dispatches commands 2, 3, 4, and 5.
The dispatch appears at `rom:04c214` through `rom:04c250`.

Messages with at most 57 bytes use one frame.
Longer messages use flag 0x10 on the first frame.
They use flag 0x20 on intermediate frames.
They use flag 0x40 on the final frame.
The transmit decisions appear at `rom:04c100` through `rom:04c12c`.

The receiver appends each payload to a 512-byte assembly buffer.
It dispatches a message only after a frame without flags 0x10 or 0x20.
These operations appear at `rom:04c296` through `rom:04c2dc`.

Command 0 and command 1 are transport control frames.
Command 1 advances the transmit offset by 57 bytes.
The receive control path appears at `rom:04c282` through `rom:04c2f8`.

## Command 3 input scan

The scan request is 57 bytes.
Its low command nibble is 3.
The message descriptor and dispatch appear at `rom:0435be` through `rom:04360e`.

Request byte 0 cycles through 8, 4, 2, and 1.
The phase update appears at `rom:04323a` through `rom:04324c`.

Request byte 56 has bit 0 set.
The initialization appears at `rom:043572` through `rom:043574`.

Response byte 56 must have bit 1 set.
The check appears at `rom:0435f4` through `rom:043600`.

Response byte 1 contains the five-bit sample and the response type.
Bits 7 through 5 select response type 0xE0 or 0xC0.
The remaining five bits contain the sample.
The checks appear at `rom:043c5e` through `rom:043c8c`.

The following map applies in native operating mode 0.
Bank numbers identify output bytes at `ram:4161` through `ram:4163`.

| Phase | Sample bit | Output bit | Primary evidence | Secondary evidence |
| ---: | ---: | --- | --- | --- |
| 8 | 0 | Bank 2 bit 2 | `rom:0437ea` through `rom:0437fe` | `rom:043a46` through `rom:043a5a` |
| 8 | 1 | Bank 0 bit 2 | `rom:0437a6` through `rom:0437b8` | `rom:043a02` through `rom:043a14` |
| 8 | 2 | Bank 0 bit 0 | `rom:0437ba` through `rom:0437c8` | `rom:043a16` through `rom:043a24` |
| 8 | 3 | Bank 0 bit 3 | `rom:043784` through `rom:043792` | `rom:0439e0` through `rom:0439ee` |
| 8 | 4 | Bank 0 bit 1 | `rom:043798` through `rom:0437a4` | `rom:0439f4` through `rom:043a00` |
| 4 | 0 | Bank 1 bit 0 | `rom:043962` through `rom:04396e` | `rom:043bd8` through `rom:043be6` |
| 4 | 1 | Bank 0 bit 6 | `rom:043924` through `rom:043936` | `rom:043b9c` through `rom:043bae` |
| 4 | 2 | Bank 0 bit 4 | `rom:04390e` through `rom:043922` | `rom:043b86` through `rom:043b9a` |
| 4 | 3 | Bank 0 bit 7 | `rom:04394c` through `rom:04395e` | `rom:043bc4` through `rom:043bd6` |
| 4 | 4 | Bank 0 bit 5 | `rom:043938` through `rom:04394a` | `rom:043bb0` through `rom:043bc2` |
| 2 | 0 | Bank 1 bit 3 | `rom:043898` through `rom:0438ac` | `rom:043b10` through `rom:043b24` |
| 2 | 1 | Bank 1 bit 7 | `rom:0438da` through `rom:0438ee` | `rom:043b52` through `rom:043b66` |
| 2 | 2 | Bank 1 bit 6 | `rom:0438f0` through `rom:043904` | `rom:043b68` through `rom:043b7c` |
| 2 | 3 | Bank 1 bit 5 | `rom:0438ae` through `rom:0438c2` | `rom:043b26` through `rom:043b3a` |
| 2 | 4 | Bank 1 bit 4 | `rom:0438c4` through `rom:0438d8` | `rom:043b3c` through `rom:043b50` |
| 1 | 0 | Bank 2 bit 5 | `rom:043802` through `rom:04381a` | `rom:043a5e` through `rom:043a76` |
| 1 | 2 | Bank 1 bit 1 | `rom:04387c` through `rom:04388e` | `rom:043ad8` through `rom:043aea` |
| 1 | 3 | Bank 2 bit 1 | `rom:043832` through `rom:043848` | `rom:043a8e` through `rom:043aa4` |
| 1 | 4 | Bank 1 bit 2 | `rom:043862` through `rom:04387a` | `rom:043abe` through `rom:043ad6` |
| 1 | 1 | Bank 2 bit 3 | Not assigned | `rom:043aec` through `rom:043b06` |

The binary stores separate primary and secondary scan workspaces.
It combines their three output banks at `rom:043982` through `rom:0439aa` and `rom:043bfa` through `rom:043c22`.

Some output bits change in operating modes other than mode 0.
Those operating-mode mappings are not implemented yet.

## Timing and failure behavior

The normal transaction deadline is 10 ms.
The deadline calculation appears at `rom:04c1ac` through `rom:04c1b6`.

The binary counts boundary failures, CRC failures, and transaction timeouts.
It changes the transport error state after repeated failures.
These paths appear at `rom:04c1ea` through `rom:04c20c` and `rom:04c304` through `rom:04c358`.

The platform startup waits 300 ms after it initializes the transport.
It then runs a routed-message probe before normal wheel negotiation.
The startup path appears at `rom:038030` through `rom:03808c`.

## Alternate UART mode

The firmware has a second UART3 configuration for an operating-mode transition.
This configuration sets U3BRG to 0xC2 and disables high-speed baud generation.
It also disables DMA5 and DMA6.
The configuration appears at `rom:04e610` through `rom:04e688`.

The operating-mode transition starts with command 5 routed messaging.
It later changes the UART mode and waits 10 ms before the next state.
This sequence appears at `rom:0382d4` through `rom:03833c`.

Command 5 is not a simple wheel status request.
Its receive handler copies 15 bytes into shared routed state and checks byte 14 for 0xAA.
The handler appears at `rom:0520ea` through `rom:05218e`.

The meanings of those 15 bytes remain unknown.
No implementation assigns firmware, temperature, uptime, or error meanings to those bytes.

## Current implementation coverage

| Area | Coverage | Next evidence work |
| --- | --- | --- |
| Normal UART3 and DMA link | Implemented | Compare interrupt recovery and error counters. |
| 64-byte frame and CRC | Implemented | Add more independent binary-derived vectors. |
| Single-frame command transfer | Implemented | Compare packet-counter and control-frame behavior. |
| Message fragmentation | Missing | Recover exact control-frame acceptance and retry behavior. |
| Startup routed probe | Missing | Recover the command 5 request data and completion states. |
| Alternate UART mode | Missing | Recover its byte transport and exit conditions. |
| Command 2 wheel negotiation | Provisional | Trace every mode, field, timeout, and authentication branch. |
| Command 3 auxiliary scan | Partial | Native mode 0 mappings are implemented. Recover aggregation masks and alternate operating-mode mappings. |
| Command 4 memory transfer | Missing | Recover access rules, ranges, and update behavior. |
| Command 5 routed messages | Missing | Recover message types and payload meanings. |

The provisional code remains subject to removal or revision.
A behavior becomes final only after this record contains direct binary evidence.
