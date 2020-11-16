#!/bin/sh
pijuice_cmd system-power-switch 0
pijuice_cmd led-blink --count 3
sudo systemctl poweroff
