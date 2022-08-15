#!/bin/bash

# Run this code as sudo

sudo iptables -t nat -A POSTROUTING -j MASQUERADE -s 192.0.2.3
sudo sysctl -w net.ipv4.ip_forward=1

../net-tools/loop-socat.sh &
sudo ../net-tools/loop-slip-tap.sh