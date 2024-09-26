#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import py_interface
import sys
from ctypes import *

np.random.seed(0)

CELL_NUM = 9
STATE_LENGTH = CELL_NUM * 2

class sEnv(Structure):
    _pack_ = 1
    _fields_ = [
        ("rsrpReward", c_uint8),
        ("rsrpMap", c_uint8 * STATE_LENGTH),
    ]

class sAct(Structure):
    _pack_ = 1
    _fields_ = [
        ("nextCellId", c_uint16),
    ]

class sInfo(Structure):
    _pack_ = 1
    _fields_ = [
        ("cellNum", c_uint32),
    ]


exp = py_interface.Experiment(1234, 4096, "Handover", "../")
exp.run(show_output=1)

var = py_interface.Ns3AIRL(1080, sEnv, sAct, sInfo)

try:
    while True:
        with var as data:
            if data == None:
                print("Not shared data, exiting.")
                break
            # Deep Learning code there
            state = list(data.env.rsrpMap)
            reward = int(data.env.rsrpReward)

            action = np.argmax(state[:CELL_NUM]) + 1
            data.act.nextCellId = c_uint16(action)

            print(f"data.env.rsrpMap: {state[:]} \n")
            print(f"serving cell: {state[CELL_NUM:].index(1) + 1}")
            print(f"data.act.nextCellId: {data.act.nextCellId}, \n")
except KeyboardInterrupt:
    print("Ctrl C")
finally:
    py_interface.FreeMemory()
