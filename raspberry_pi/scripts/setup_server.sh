#!/bin/bash
set -e

SSID="PSP_NET_BRIDGE"
PASSPHRASE="password123"
WLAN_IFACE="wlan0"
STATIC_IP="192.168.2.1"
SERVER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_USER="${SUDO_USER:-pi}"
SERVER_GROUP="$(id -gn "$SERVER_USER")"

echo "[1/5] Installing dependencies..."
sudo apt-get update
sudo apt-get install -y hostapd dnsmasq python3 ffmpeg python3-opencv

echo "[2/5] Configuring static IP..."
sudo bash -c "cat >/etc/dhcpcd.conf" <<EOF
interface $WLAN_IFACE
    static ip_address=$STATIC_IP/24
    nohook wpa_supplicant
EOF
sudo systemctl restart dhcpcd

echo "[3/5] Configuring dnsmasq..."
sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.backup
sudo bash -c "cat >/etc/dnsmasq.conf" <<EOF
interface=$WLAN_IFACE
dhcp-range=192.168.2.10,192.168.2.50,255.255.255.0,24h
EOF

echo "[4/5] Configuring hostapd..."
sudo bash -c "cat >/etc/hostapd/hostapd.conf" <<EOF
interface=$WLAN_IFACE
driver=nl80211
ssid=$SSID
hw_mode=g
channel=6
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=$PASSPHRASE
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF
sudo sed -i "s|#DAEMON_CONF=\"\"|DAEMON_CONF=\"/etc/hostapd/hostapd.conf\"|" /etc/default/hostapd

echo "[5/5] Starting services..."
sudo systemctl unmask hostapd
sudo systemctl enable hostapd dnsmasq
sudo systemctl start hostapd dnsmasq

echo "[*] Installing PSP server service..."
sudo install -d -o "$SERVER_USER" -g "$SERVER_GROUP" "$SERVER_DIR/logs"
sudo tee /etc/systemd/system/psp-rpi-car.service >/dev/null <<EOF
[Unit]
Description=PSP Raspberry Pi Car Server
Wants=hostapd.service dnsmasq.service
After=network.target hostapd.service dnsmasq.service

[Service]
Type=simple
User=$SERVER_USER
WorkingDirectory=$SERVER_DIR
ExecStart=/usr/bin/python3 $SERVER_DIR/server.py --config $SERVER_DIR/server.ini
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF
sudo systemctl daemon-reload
sudo systemctl enable --now psp-rpi-car.service

echo "Setup complete"
echo "Host SSID: '$SSID'"
echo "Host password: '$PASSPHRASE'"
echo "Server config: $SERVER_DIR/server.ini"
echo "Restart server: sudo systemctl restart psp-rpi-car.service"
