# ESPHome component for NSPanel Lovelace UI

This repository contains ESPHome component that can be used as an alternative to the official
Tasmota driver provided in [NSPanel Lovelace UI](https://github.com/joBr99/nspanel-lovelace-ui)
repository. It still uses the same AppDaemon backend and TFT driver but the MQTT commands
from the AppDaemon are forwarded to a custom component that translates them to the protocol
used by the display's Nextion firmware, providing the same UX for users that prefer ESPHome
over Tasmota.

Please note that this code is currently in a very early stage of development, based on code
~~stolen from~~ based on the abandoned code from joBr99's repo (which is actually based on an
[outstanding PR for the stock NSPanel FW](https://github.com/esphome/esphome/pull/2702)) and
official [ESPHome Nextion component](https://github.com/esphome/esphome/tree/dev/esphome/components/nextion)
(TFT FW upload). Once the code stabilizes (no ETA), properly semver-tagged releases will likely
appear.

## Usage

Add reference to this repository to the `external_components` definitions:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/sairon/esphome-nspanel-lovelace-ui
      ref: dev
    components: [nspanel_lovelace]
```

To handle messages received via MQTT from the AppDaemon, first create the `mqtt` component with the address of your
MQTT broker:

```yaml
mqtt:
  broker: 192.168.1.1
```

Then initialize the `nspanel_lovelace` component by adding the following line:

```yaml
nspanel_lovelace:
```

There are several options you can use:

- `id`: ID of the component used in lambdas
- `mqtt_recv_topic`: change if you are using different `panelRecvTopic` in your AppDaemon config
- `mqtt_send_topic`: change if you are using different `panelSendTopic` in your AppDaemon config
- `on_incoming_message`: action called when a message is received from the NSPanel firmware (e.g. on a touch event)
- `use_missed_updates_workaround`: use workaround for [missed status updates](https://github.com/sairon/esphome-nspanel-lovelace-ui/issues/8) - introduce short delay in internal ESPHome-NSPanel communication. Defaults to `true`, usually there should be no need to disable this option.
- `update_baud_rate`: baud rate to use when updating the TFT firmware, defaults to `921600`. Set to `115200` if you encounter any issues during the updates.

### TFT firmware update

Firmware of the TFT can be updated using the `upload_tft` function - for that you can create a service that can
be triggered from the Home Assistant when needed (make sure you have defined an ID for the NSPanel component):

```yaml
api:
  - service: upload_tft
    variables:
      url: string
    then:
      - lambda: |-
          id(nspanel).upload_tft(url);
```

### Other functions

There are currently few more public functions which can be useful for debugging or writing custom automations
for controlling the display:

- `send_custom_command(command)`: send NSPanel Lovelace UI custom command (`0x55 0xBB` + len prefix, CRC suffix)
- `send_nextion_command(command)`: send Nextion command (suffixed with `0xFF 0xFF 0xFF`)
- `start_reparse_mode()`: start Nextion reparse mode (component will also stop handling data from the display)
- `exit_reparse_mode()`: exit Nextion reparse mode

### Example configuration

An example configuration was added on [example-nspanel-config.yml](example-nspanel-config.yml).
On this configuration, we expose multiple services to Home Assistant:

- upload_tft: Used to upload the firmware to the panel, you just need to add the [URL of the firmware](https://docs.nspanel.pky.eu/prepare_nspanel/#flash-firmware-to-nextion-screen) (only the URL)
- play_rtttl: play Nokia ringtones on the buzzer of the panel, example `Mario:d=4,o=5,b=100:32p,16e6,16e6,16p,16e6,16p,16c6,16e6,16p,16g6,8p,16p,16g,8p,32p,16c6,8p,16g,8p,16e,8p,16a,16p,16b,16p,16a#,16a,16p,16g,16e6,16g6,16a6,16p,16f6,16g6,16p,16e6,16p,16c6,16d6,16b,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,8d#6,16p,16d6,8p,8c6`
- wake: to wake the screen when is off
- show_screensaver: it shows the screensaver
- navigate_to_page: navigate to the page from the parameter. (The name of the page is created using the type of the card and the key value, like `cardMedia_bedroom`)
- disable_screensaver: make sure it won't show screensaver after the timeout (useful for showing something all the time on the screen, like a media card while it's playing)
- enable_screensaver: enable the screensaver after it was disabled
- dim_0_to_100: dim screen brightness with values from 0 to 100
- show_entity: show the card of an entity on the screen. It requires the id of the entity, ex `light.bedroom` and title, ex `Bedroom Light`
- notify_on_screensaver: show a notification on the screensaver with 2 lines, it can receive both lines. The notification will disappear when you tap on the screensaver
- notify_fullscreen: show a full-screen notification with title description and 2 buttons, and it plays a sound on the buzzer. The buttons are not actionable yet

On the example configuration under the switch on_turn_off/on_turn_on there are a feature implemented to show an entity on the screen when the button is press and show the screensaver when the switch is off

## License

Code in this repository is licensed under the [ESPHome License](LICENSE) (MIT for Python sources, GPL for C++).
