# Radar Watch Face for M5StickC

A futuristic, landscape watch face for the original M5StickC. It shows a
12-hour clock in Arizona time and an animated radar sweep. Press Button A to
send a pulse across the radar.

The display uses `M5.Lcd.setRotation(3)`. The clock converts UTC read from the
onboard BM8563 RTC to Arizona's UTC-7 time, which does not observe daylight
saving time. The RTC must be set to UTC; the sketch does not connect to Wi-Fi or
set the clock automatically.

To reduce flicker, the background and interface are drawn once at startup.
Only the clock area is redrawn when its displayed time changes, and radar
animation updates erase and redraw the sweep and pulse while restoring the
radar grid.

## Upload

1. Install the **M5StickC** board support package and the classic **M5StickC**
   Arduino library.
2. Open `retro-watch.ino` in Arduino IDE, select **M5StickC** as the board, then
   build and upload.

## Controls

- **Button A**: animate an expanding radar pulse.
- **Radar**: continuously sweep with highlighted detected targets.
- **Clock**: display Arizona local time in 12-hour format with an AM/PM marker
  and a colon separator.
