#!/bin/bash
set -e

SSID="PSP_NET_BRIDGE"
PASSPHRASE="password123"
WLAN_IFACE="wlan0"
STATIC_IP="192.168.2.1"

echo "[1/5] Installing dependencies..."
sudo apt-get update
sudo apt-get install -y hostapd dnsmasq python3

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

echo "[*] Launching PSP server..."
nohup python3 /home/pi/psppi/server.py > /var/log/psppi.log 2>&1 &

echo "Setup complete"
echo "Host SSID: '$SSID'"
echo "Host password: '$PASSPHRASE'"
