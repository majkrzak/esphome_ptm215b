EnOcean PTM 215B component for ESPHome
======================================


PTM 215B from EnOcean is a BLE pushbutton transmitter module powered with the energy from the press/release action. This component provides implementation according to [the user manual](https://www.enocean.com/wp-content/uploads/downloads-produkte/en/products/enocean_modules_24ghz_ble/ptm-215b/user-manual-pdf/PTM-215B-User-Manual.pdf).


Installation
------------

This is an external ESPHome component and therefore have to be explicitly installed before use.

```yaml
external_components:
  - source: github://majkrzak/esphome_ptm215b
    components: [ ptm215b ]
```


Configuration
-------------

```yaml
binary_sensor:
  - platform: ptm215b
    mac_address: 11:11:11:11:11:11
    security_key: 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
    any:
      ...
    a0:
      ...
    a1:
      ...
    b0:
      ...
    b1:
      ...
```


### BLE scan parameters

To set up the device it is necessary to correctly configure the `esp32_ble_tracker` `scan_parameters`.
According to the user manual (Section 9.4) minimum scan window is 23 ms while maximum interval is 37 ms. According to my observations this results in lost packets from time to time. Much more consistent reception is achieved while setting those values to 25 and 15 respectively. In terms of ESPHome configuration it looks like:

```yaml
esp32_ble_tracker:
  scan_parameters:
    active: false
    interval: 40ms
    window: 25ms
    duration: 100d
```


### Buttons

Component exposes 5 independent binary sensors, 1 for each of A0, A1, B0, B1 buttons and one for the energy bar.
Combined dependent sensors (like A1 and B1) are not yet implemented, although implementation is in progress.


### Commissioning

Although the component works without `security key` provided, it is recommended to provide such, so the signatures could be verified.

The key, according to the manual (Section 5), can be obtained in three ways. Through the NFC interface, out of QR code, or out of the commissioning telegram. The latest method is implemented in this module.

> To enter commissioning mode, start by selecting one button contact of PTM 215B. Any but-
ton of PTM 215B (A0, A1, B0, B1) can be used.
> 1. Press and hold the selected button together with the energy bar for more than 7 seconds before releasing it
> 2. Press the selected button together with the energy bar quickly (hold for less than 2 seconds)
> 3. Press and hold the selected button together with the energy bar again for more than 7 seconds before releasing it
>
> Pressing any key except the button used for entry into commissioning mode will cause PTM 215B to stop transmitting commissioning telegrams and return to normal data telegram transmission.

Component will report received key in the logs on info level.


### Factory reset

Current implementation only covers the default device configuration, so it may be necessary to factory reset the device before use.

> In order to execute such factory reset, the rocker(s) and the switch housing have to be removed from the PTM 215B module.
> Then, all four button contacts (A0, A1, B0 and B1) have to be pressed at the same time while the energy bow is pressed down.
> The energy bow must then be held at the down position for at least 10 seconds before being released.
> The button contacts A0, A1, B0 and B1 can be released at any time after pressing the energy bow down, i.e. it is no requirement to hold them as well for at least 10 seconds.
