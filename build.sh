#!/bin/sh

make

teensy_loader_cli -mmcu=mk20dx256 -w -s -v ./bin/robot-interface.hex
