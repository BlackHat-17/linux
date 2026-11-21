#!/bin/bash

ifconfig enx525400123457 up
ifconfig enx525400123457 10.0.2.15 netmask 255.255.255.0 up
route add default gw 10.0.2.2
ifconfig enx525400123457
