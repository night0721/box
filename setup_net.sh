#!/bin/sh
PID=$1

# Only setup bridge if it doesn't exist
if [ ! -d /sys/class/net/br0 ]; then
    sudo ip link add br0 type bridge
    sudo ip addr add 192.168.10.1/24 dev br0
    sudo ip link set br0 up
fi

# Create Veth pair
sudo ip link add veth-host type veth peer name veth-guest

# Attach host-side to bridge
sudo ip link set veth-host master br0
sudo ip link set veth-host up

# PLUG IT IN: Move the guest end into the container's namespace
sudo ip link set veth-guest netns $PID
