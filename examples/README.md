# ESPHome configuration examples

## Basic config

Minimal configuration with configuration of all peripherals is in [basic-config.yml](basic-config.yml). Buttons are
coupled with respective relays and two Home Assistant serviecs are exposed - `upload_tft` for an explicit upgrade
of the display firmware (can be used for the initial reflash) and `play_rtttl` for playing [RTTTL melodies](https://esphome.io/components/rtttl.html).

After the NSPanel Lovelace UI firmware is flashed for the first time, the display should show an actionable
notification for upgrade to newer version when NSPanel Lovelace UI Backend is updated from HACS if
the `updateMode` options is set to `auto-notify`, or should be upgraded automatically if it's set to `auto`.

## Advanced configuration

More complex example was contributed by [@spike886](https://github.com/spike886), you can find it in
[advanced-config.yml](advanced-config.yml).

In this configuration, we expose multiple services to Home Assistant:

- `upload_tft`: used to upload the firmware to the panel, requires [URL of the firmware](https://docs.nspanel.pky.eu/prepare_nspanel/#flash-firmware-to-nextion-screen) (only the URL)
- `play_rtttl`: play Nokia ringtones on the buzzer of the panel, e.g. `Mario:d=4,o=5,b=100:32p,16e6,16e6,16p,16e6,16p,16c6,16e6,16p,16g6,8p,16p,16g,8p,32p,16c6,8p,16g,8p,16e,8p,16a,16p,16b,16p,16a#,16a,16p,16g,16e6,16g6,16a6,16p,16f6,16g6,16p,16e6,16p,16c6,16d6,16b,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,8d#6,16p,16d6,8p,8c6`
- `wake`: wake the screen when it is off
- `show_screensaver`: show the screensaver
- `navigate_to_page`: navigate to the page from the parameter. (The name of the page is created using the type of the card and the key value, like `cardMedia_bedroom`)
- `disable_screensaver`: make sure it won't show screensaver after the timeout (useful for showing something all the time on the screen, like a media card while it's playing)
- `enable_screensaver`: enable the screensaver after it was disabled
- `dim_0_to_100`: dim screen brightness with values from 0 to 100
- `show_entity`: show the card of an entity on the screen. It requires the id of the entity, ex `light.bedroom` and title, ex `Bedroom Light`
- `notify_on_screensaver`: show a notification on the screensaver with 2 lines, it can receive both lines. The notification will disappear when you tap on the screensaver
- `notify_fullscreen`: show a full-screen notification with title description and 2 buttons, and it plays a sound on the buzzer. The buttons are not actionable yet

This configuration also contains configuration of the `switch` entity that shows how to show the entity
on the display when the light is turned on, and show the screensaver when the light is turned off.
