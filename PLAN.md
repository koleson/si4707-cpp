recently completed:
- 2/3:  fixed long-standing issue where Wiznet MQTT publisher would fail to get a DHCP IP address.
  - probably the physical medium was not ready.  maybe there's a way to check that?
- 2/3:  fixed long-standing issue where Si4707 would report a valid tune but 0 RSSI / 0 SNR
  - AN332 notes that the external crystal oscillator needs time to stabilize.  adding a 500ms delay improved this bug, and improved overall SNR numbers by ~5-9

bugs:
- none at the moment! (hahaha, okay, none that i am noting here)

in progress:
- si4707:  ASQ (alert tone) status mechanisms
  - attention signal status is good indicator that received SAME header is as good as it will get
  - 2/3:  have the command/response and printing ready
  - TODO:  use response to change states in the state machine (after validation, hopefully on 7 Feb)
- include confidence data in MQTT output (reverse memcopy i think was tested on 1 Feb - include in mqtt now)
- move pi pico SDK calls out of si4707.c/.h to facilitate HAL and testing
- state machine
  - most states working (idle, receiving header, EOM)
  - need to get alert tone and broadcast message states working
    - ASQ work enables this

short-term:
- move mqtt secrets, etc. to uncommitted settings override header similar to Tasmota's
- include git commit hash (and clean/dirty status) in binary/MQTT messages
- unify heartbeat interval logic (i think i'm updating it in several places right now)
- clean up CMakeLists.txt with variants
   - https://github.com/raspberrypi/pico-examples/blob/master/cmake/build_variants/CMakeLists.txt
- send SOM / EOM message to MQTT aside from heartbeet

long-term:
- watchdog timer for receive / alert / broadcast / state
  - 5 minutes, for example, seems reasonable for most situations
- allow tuning arbitrary frequency (within allowed) via MQTT message
- allow resetting device via MQTT message
- si4707:  allocate SAME buffers based on MSGLEN
- mqtt-publisher: add DNS lookup of mqtt broker hostname
- si4707-cpp.c / mqtt-publisher.c: emit MQTT of SAME state on SAME state changes rather than fixed interval
  - need to store and have convenience comparison of SAME state
  - OR:  use a state machine (IN PROGRESS)


completion log:
- 12/27:  si4707.c: validated number of requests needed for a given MSGLEN 
- 12/27:  si4707-cpp.c:  clear SAME message buffer and interrupts after EOM (and some time.)
  - EOMDET = 1 at end of audio broadcast following activation
  - STATE flips between 0 and 1 several times - once for each EOM marker (NNNN)
    - each preamble/EOM marker takes a bit less than 0.4 seconds to transmit
    - 1 second silence between each marker
    - 3 seconds after first EOM seems like an adequate trap
      - assumes EOMDET is only set to 1 at completion of 1 second silence + 0.4 second EOM marker
    - prefer not to count EOMs since they might not all be received correctly
    - if we got a header and get an EOM, we can be fairly sure we are done for that instance
    - if 3 seconds has elapsed from first EOM, and state == 0, reset interrupts and clear SAME buffer