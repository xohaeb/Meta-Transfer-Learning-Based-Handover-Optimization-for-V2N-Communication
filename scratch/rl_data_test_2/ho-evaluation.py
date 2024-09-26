#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import numpy as np
import torch
from torch import nn
import torch.nn.functional as F
from torch.optim.lr_scheduler import StepLR
import copy
import matplotlib
import matplotlib.pyplot as plt
from helper import QNetwork, Memory, evaluate, train, select_action, update_parameters, device
from py_interface import *
from ctypes import *

np.random.seed(0)

CELL_NUM = 8
STATE_LENGTH = CELL_NUM * 2

class sEnv(Structure):
    _pack_ = 1
    _fields_ = [
        ("rsrp_Reward", c_uint8),
        ("rsrp_Map", c_uint8*STATE_LENGTH)
    ]

class sAct(Structure):
    _pack_ = 1
    _fields_ = [
        ("next_CellId", c_uint16),
    ]


class sInfo(Structure):
    _pack_ = 1
    _fields_ = [
        ("CellNum", c_uint32),
    ]

def main(gamma=0.99, lr=5e-6, lr_min = 1e-8, min_episodes=20, epsilon=0.1, epsilon_decay=0.998, epsilon_min=0.02, update_step=100, batch_size=1000, update_repeats=50,
         num_episodes=2000, seed=42, max_memory_size=50000, lr_gamma=0.9, lr_step=100, measure_step=100, measure_repeats=5, hidden_dim=128, 
         cellNum = 8, timeSpan = 300, timeStep = 0.2, nodeNum = 18):
    exp = Experiment(1234, 4096, "Handover", "../")
    exp.run(show_output=1)

    var = Ns3AIRL(1080, sEnv, sAct, sInfo)
    
    Q_eval = QNetwork(action_dim=cellNum, state_dim=2*cellNum,
                                    hidden_dim=hidden_dim).to(device)
    Q_target = QNetwork(action_dim=cellNum, state_dim=2*cellNum,
                                    hidden_dim=hidden_dim).to(device) 
    
    Q_eval.load_state_dict(torch.load("./training_model.pth"))
    Q_target.load_state_dict(torch.load("./target_model.pth"))
    for param in Q_target.parameters():
        param.requires_grad = False
    optimizer = torch.optim.AdamW(Q_eval.parameters(), lr=lr)
    scheduler = StepLR(optimizer, step_size=lr_step, gamma=lr_gamma)
    
    perform_overall = 0
    try:
        while True:
            with var as data:
                if data == None:
                    break
                # Deep Learning code there
                state = np.array(data.env.rsrp_Map)
                serving_CellID = np.where(state[CELL_NUM:] == 1)[0][0]
                
                Q_eval.eval()

                if np.max(state) == state[serving_CellID]:
                    action = serving_CellID + 1
                else:
                    state_tensor = torch.Tensor(state).to(device)
                    with torch.no_grad():
                        values = Q_eval(state_tensor)
                    action = np.argmax(values.cpu().numpy()) + 1

                reward = int(data.env.rsrp_Reward)  # reward is the next time step's rsrp of target cell
                # reward = post_state[int(action)]
                # save state, action, reward sequence
                perform_overall += reward 

                data.act.next_CellId = c_uint16(action)

    except KeyboardInterrupt:
        print('Ctrl C')
    finally:
        FreeMemory()
    

if __name__ == "__main__":
    main()
