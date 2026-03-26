#!/bin/bash

echo "Configuring BLE for high-interference environment..."

# Backup original config
sudo cp /etc/bluetooth/main.conf /etc/bluetooth/main.conf.backup

# Check if LE section exists
if grep -q "\[LE\]" /etc/bluetooth/main.conf; then
    echo "LE section already exists, updating values..."
    # Remove old LE section and add new one
    sudo sed -i '/\[LE\]/,/^$/d' /etc/bluetooth/main.conf
fi

# Add optimized LE configuration
echo "" | sudo tee -a /etc/bluetooth/main.conf
echo "[LE]" | sudo tee -a /etc/bluetooth/main.conf
echo "MinConnectionInterval=6" | sudo tee -a /etc/bluetooth/main.conf
echo "MaxConnectionInterval=12" | sudo tee -a /etc/bluetooth/main.conf
echo "ConnectionLatency=0" | sudo tee -a /etc/bluetooth/main.conf
echo "ConnectionSupervisionTimeout=200" | sudo tee -a /etc/bluetooth/main.conf

echo "Restarting Bluetooth service..."
sudo systemctl restart bluetooth

sleep 3

echo "BLE configuration complete!"