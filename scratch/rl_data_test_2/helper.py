import numpy as np
import torch
from torch import nn
import torch.nn.functional as F
from torch.optim.lr_scheduler import StepLR
import random
import collections
import copy
from numba import jit
device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")

class QNetwork(nn.Module):
    def __init__(self, action_dim, state_dim, hidden_dim):
        super(QNetwork, self).__init__()

        # self.layer_1 = nn.Linear(state_dim, hidden_dim)
        # self.layer_2 = nn.Linear(hidden_dim, hidden_dim)
        # self.layer_3 = nn.Linear(hidden_dim, action_dim)
        self.layer_1 = nn.Linear(state_dim, 2*hidden_dim)
        self.layer_2 = nn.Linear(2*hidden_dim, hidden_dim)
        self.layer_3 = nn.Linear(hidden_dim, int(hidden_dim/2))
        self.layer_4 = nn.Linear(int(hidden_dim/2), action_dim)
        # self.layer_1 = nn.Linear(state_dim, 2*hidden_dim)
        # self.layer_2 = nn.Linear(2*hidden_dim, hidden_dim)
        # self.layer_3 = nn.Linear(hidden_dim,int(hidden_dim/2))
        # self.layer_4 = nn.Linear(int(hidden_dim/2), hidden_dim)
        # self.layer_5 = nn.Linear(hidden_dim, action_dim)

    def forward(self, inp):
        x = F.gelu(self.layer_1(inp))
        x = F.gelu(self.layer_2(x))
        x = F.gelu(self.layer_3(x))
        x = self.layer_4(x)
        # x = F.gelu(self.layer_1(inp))
        # x = F.gelu(self.layer_2(x))
        # x = F.gelu(self.layer_3(x))
        # x = F.gelu(self.layer_4(x))
        # x = self.layer_5(x)
        return x

"""
memory to save the state, action, reward sequence from the current episode. 
"""
class Memory:
    def __init__(self, len):
        self.rewards = collections.deque(maxlen=len)
        self.state = collections.deque(maxlen=len)
        self.action = collections.deque(maxlen=len)
        self.is_done = collections.deque(maxlen=len)

    def update(self, state, action, reward, done):
        # if the episode is finished we do not save to new state. Otherwise we have more states per episode than rewards
        # and actions whcih leads to a mismatch when we sample from memory.
        if not done:
            self.state.append(state)
        self.action.append(action)
        self.rewards.append(reward)
        self.is_done.append(done)

    def sample(self, batch_size):
        """
        sample "batch_size" many (state, action, reward, next state, is_done) datapoints.
        """
        n = len(self.is_done)
        idx = random.sample(range(0, n-1), batch_size)

        return torch.Tensor(self.state)[idx].to(device), torch.LongTensor(self.action)[idx].to(device), \
               torch.Tensor(self.state)[1+np.array(idx)].to(device), torch.Tensor(self.rewards)[idx].to(device), \
               torch.Tensor(self.is_done)[idx].to(device)

    def reset(self):
        self.rewards.clear()
        self.state.clear()
        self.action.clear()
        self.is_done.clear()
        
def select_action(model, cellNum, state, eps):
    state = torch.Tensor(state).to(device)
    with torch.no_grad():
        values = model(state)
    # select a random action wih probability eps
    if random.random() < eps:
        action = np.random.randint(0, cellNum)
    else:
        action = np.argmax(values.cpu().numpy())

    return action


def train(batch_size, current, target, optim, memory, gamma):

    states, actions, next_states, rewards, is_done = memory.sample(batch_size)

    q_values = current(states)

    next_q_values = current(next_states)
    next_q_state_values = target(next_states)

    q_value = q_values.gather(1, actions.unsqueeze(1)).squeeze(1)
    next_q_value = next_q_state_values.gather(1, torch.max(next_q_values, 1)[1].unsqueeze(1)).squeeze(1)
    expected_q_value = rewards + gamma * next_q_value * (1 - is_done)
    
    criterion = nn.MSELoss()
    loss = criterion(q_value, expected_q_value.detach())
    # print(np.size(loss1))
    # print(loss1)
    # loss2 = (q_value - expected_q_value.detach()).pow(2).mean()
    # print(loss2)
    # print('----')
    # print(loss1 == loss2)
    # """
    # criterion = nn.GaussianNLLLoss()
    # var = (torch.ones(batch_size, requires_grad=True) * 4).to(device) # sigma as specified in 3gpp Uma
    # loss = criterion(q_value, expected_q_value.detach(), var)
    # """
    # criterion = nn.SmoothL1Loss()
    # loss = criterion(q_value, expected_q_value.detach())
    error = q_value - expected_q_value.detach()
    optim.zero_grad()
    loss.backward()
    optim.step()
    return error.detach().cpu().numpy()                      

def evaluate(Qmodel, rsrp_report, serving_cells_origin, nodeNum, cellNum, total_steps, repeats, train="clean"):
    """
    Runs a greedy policy with respect to the current Q-Network for "repeats" many episodes. Returns the average
    episode reward.
    """
    cell_one_hot = F.one_hot(torch.arange(0, cellNum)).numpy()
    Qmodel.eval()
    perform_overall = 0
    perform_per_node = np.zeros(nodeNum)
    action_record = open(f"actions_{train}.txt", "w")
    for i in range(repeats):
        for node in range(nodeNum):

            serving_cells = copy.deepcopy(serving_cells_origin)
            state = np.array(rsrp_report[node].loc[1, 'rsrp'])
            state = np.append(state, cell_one_hot[int(serving_cells[node])])
            state = np.reshape(state, [2*cellNum, ])
            done = False
            for step in range(total_steps-2):
                if np.argmax(state) == serving_cells[node]:
                    action = copy.deepcopy(serving_cells[node])
                else:
                    state_tensor = torch.Tensor(state).to(device)
                    with torch.no_grad():
                        values = Qmodel(state_tensor)
                    action = np.argmax(values.cpu().numpy())

                post_state = np.array(rsrp_report[node].loc[step+2, 'rsrp'])
                post_state = np.append(post_state, cell_one_hot[int(action)])
                post_state = np.reshape(post_state, [2*cellNum, ])

                reward = (post_state[int(action)] - state[int(np.argmax(state))]) # reward is the next time step's rsrp of target cell
                # reward = post_state[int(action)]
                if i >= repeats - 3:
                    action_record.write(f"node{node}, {step/5}, {int(serving_cells[node])} to {int(action)} \n")
                # save state, action, reward sequence
                perform_overall += reward 
                perform_per_node[node] += reward
                serving_cells[node] = copy.deepcopy(action)
                state = copy.deepcopy(post_state)
                serving_cells[node] = action
    Qmodel.train()
    action_record.close()
    return perform_overall/repeats, perform_per_node/repeats


def update_parameters(current_model, target_model):
    target_model.load_state_dict(current_model.state_dict())
    