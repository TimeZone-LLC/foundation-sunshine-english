#!/bin/sh

# User Service
systemctl --user stop sunshine.service
rm -f "$HOME/.config/systemd/user/sunshine.service"
systemctl --user daemon-reload
echo "Sunshine User Service has been removed."

# Remove rules
flatpak-spawn --host pkexec rm -f /etc/modules-load.d/60-sunshine.conf
flatpak-spawn --host pkexec rm -f /etc/udev/rules.d/60-sunshine.rules
flatpak-spawn --host pkexec udevadm control --reload-rules
echo "Input rules removed."
