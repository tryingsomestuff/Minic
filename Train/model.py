import chess
import halfka
import torch
import numpy as np
from torch import nn
import torch.nn.functional as F
import pytorch_lightning as pl
import struct

netversion = struct.unpack('!f', bytes.fromhex('c0ffee03'))[0]

withFactorizer = True

# CARE !! 
# change requiered in
# void fill_entry(FeatureSet<Ts...>, int i, const TrainingDataEntry& e)
# if this is changed
nphase = 2
BASE = 384
L1 = 8
L2 = 8
L3 = 8

def piece_position(i):
  return i % halfka.state_numel()

class FactoredBlock(nn.Module):
  def __init__(self, func, output_dim):
    super(FactoredBlock, self).__init__()
    self.f = torch.tensor([func(i) for i in range(halfka.numel())], dtype=torch.long)
    self.inter_dim = 1 + self.f.max()
    self.weights = nn.Parameter(torch.zeros(self.inter_dim, output_dim))

  def virtual(self):
    with torch.no_grad():
      identity = torch.tensor([i for i in range(halfka.numel())], dtype=torch.long)
      conversion = torch.sparse.FloatTensor(
        torch.stack([identity, self.f], dim=0),
        torch.ones(halfka.numel()),
        size=torch.Size([halfka.numel(), self.inter_dim])).to(self.weights.device)
      return (conversion.matmul(self.weights)).t()

  def factored(self, x):
    N, D = x.size()
    assert D == halfka.numel()

    batch, active = x._indices()
    factored = torch.gather(self.f.to(x.device), dim=0, index=active)
    x = torch.sparse.FloatTensor(
      torch.stack([batch, factored], dim=0), 
      x._values(),
      size=torch.Size([N, self.inter_dim])).to(x.device).to_dense()
    return x

  def forward(self, x):
    x = self.factored(x)
    return x.matmul(self.weights)

class ClippedPReLU(nn.Module):

    __constants__ = ['num_parameters']
    num_parameters: int

    def __init__(self, num_parameters: int = 1, init: float = 0.25,
                 device=None, dtype=None) -> None:
        factory_kwargs = {'device': device, 'dtype': dtype}
        self.num_parameters = num_parameters
        super().__init__()
        self.init = init
        self.weight = nn.Parameter(torch.empty(num_parameters, **factory_kwargs))
        self.reset_parameters()

    def reset_parameters(self):
        torch.nn.init.constant_(self.weight, self.init)

    def forward(self, input: torch.Tensor) -> torch.Tensor:
        return torch.min(F.prelu(input, self.weight), torch.ones(self.num_parameters).to(input.device))

    def extra_repr(self) -> str:
        return f'num_parameters={self.num_parameters}'


class FeatureTransformer(nn.Module):
  def __init__(self, funcs, base_dim):
    super(FeatureTransformer, self).__init__()
    self.factored_blocks = nn.ModuleList([FactoredBlock(f, base_dim) for f in funcs])
    self.affine = nn.Linear(halfka.numel(), base_dim)

  def virtual_bias(self):
    return self.affine.bias.data

  def virtual_weight(self):
    return self.affine.weight.data + sum([block.virtual() for block in self.factored_blocks])

  def forward(self, x):
    return self.affine(x) + sum([block(x) for block in self.factored_blocks])


class NNUE(pl.LightningModule):
  """
  lambda_ = 0.0 - purely based on game results
  lambda_ = 1.0 - purely based on search scores
  """
  def __init__(self, lambda_=1.0):
    super(NNUE, self).__init__()
    funcs = [piece_position,]

    if withFactorizer:
      # with factorization
      self.white_affine = FeatureTransformer(funcs, BASE)
      self.black_affine = FeatureTransformer(funcs, BASE)
    else:
      # without factorization
      self.white_affine = nn.Linear(halfka.numel(), BASE)
      self.black_affine = nn.Linear(halfka.numel(), BASE)
    #self.activationIW = ClippedPReLU(BASE)
    #self.activationIB = ClippedPReLU(BASE)

    # dropout seems necessary when using factorizer to avoid over-fitting
    self.d0 = nn.Dropout(p=0.05)
    self.fc0 = nn.Linear(2*BASE, L1 * nphase)
    #self.activation0 = ClippedPReLU(L1 * nphase)

    self.d1 = nn.Dropout(p=0.1)
    self.fc1 = nn.Linear(L1, L2 * nphase)
    #self.activation1 = ClippedPReLU(L2 * nphase)

    self.d2 = nn.Dropout(p=0.1)
    self.fc2 = nn.Linear(L2 + L1, L3 * nphase)
    #self.activation2 = ClippedPReLU(L3 * nphase)
    
    self.d3 = nn.Dropout(p=0.1)
    self.fc3 = nn.Linear(L3 + L2 + L1,  1 * nphase)

    self.lambda_ = lambda_

    self.idx_offset = None

  def forward(self, us, them, white, black, phase):

    if self.idx_offset == None or self.idx_offset.shape[0] != phase.shape[0]:
        self.idx_offset = torch.arange(0, phase.shape[0] * nphase, nphase, device=phase.device)

    indices = phase.flatten() + self.idx_offset

    if len(white.size()) > 2: # data are from pydataloader
      w__ = halfka.half_ka(white, black)
      b__ = halfka.half_ka(black, white)
      w_ = self.white_affine(w__)
      b_ = self.black_affine(b__)
    else: # sparse data from ccdataloader
      w_ = self.white_affine(white)
      b_ = self.black_affine(black)
    
    # input layer
    #w_ = self.activationIW(w_)
    #b_ = self.activationIB(b_)
    #base = us * torch.cat([w_, b_], dim=1) + (1.0 - us) * torch.cat([b_, w_], dim=1)
    base = torch.clamp(us * torch.cat([w_, b_], dim=1) + (1.0 - us) * torch.cat([b_, w_], dim=1),0,1)
    
    if withFactorizer:
      base = self.d0(base)
    y0 = torch.clamp(self.fc0(base),0,1)
    #y0 = self.fc0(base)
    #y0 = self.activation0(y0)
    y0 = y0.view(-1, L1)[indices]

    if withFactorizer:
      y0 = self.d1(y0)
    y1 = torch.clamp(self.fc1(y0),0,1)
    #y1 = self.fc1(y0)
    #y1 = self.activation1(y1)
    y1 = y1.view(-1, L2)[indices]
    y1 = torch.cat([y0, y1], dim=1)

    if withFactorizer:
      y1 = self.d2(y1)
    y2 = torch.clamp(self.fc2(y1),0,1)
    #y2 = self.fc2(y1)
    #y2 = self.activation2(y2)
    y2 = y2.view(-1, L3)[indices]
    y2 = torch.cat([y1, y2], dim=1)

    if withFactorizer:
      y2 = self.d3(y2)
    y3 = self.fc3(y2)
    y3 = y3.view(-1, 1)[indices]

    return y3

  def step_(self, batch, batch_idx, loss_type):
    us, them, white, black, outcome, score, phase = batch
  
    #from SF values, shall be tuned
    net2score = 600
    in_scaling = 410
    out_scaling = 361
    offset = 270

    t = outcome

    # in end-game take outcome into account
    if nphase > 1:
      phase_scaling = phase * 1.0 / (nphase - 1)
    else:
      phase_scaling = 1.0
    phased_lambda_outcome = phase_scaling * (1.0 - self.lambda_)

    scorenet = self(us, them, white, black, phase) * net2score

    # simple Minic stuff
    q = (self(us, them, white, black, phase) * net2score / out_scaling).sigmoid()
    p = (score / in_scaling).sigmoid()
    pt = p * (1.0 - phased_lambda_outcome) + t * phased_lambda_outcome
    loss = torch.pow(torch.abs(pt - q), 2.6).mean()

    # From SF
    #q  = ( scorenet - offset) / in_scaling  # used to compute the chance of a win
    #qm = (-scorenet - offset) / in_scaling  # used to compute the chance of a loss
    #qf = 0.5 * (1.0 + q.sigmoid() - qm.sigmoid())  # estimated match result (using win, loss and draw probs).

    #p  = ( score - offset) / out_scaling
    #pm = (-score - offset) / out_scaling
    #pf = 0.5 * (1.0 + p.sigmoid() - pm.sigmoid())

    #pt = pf * (1.0 - phased_lambda_outcome) + t * phased_lambda_outcome

    #loss = torch.pow(torch.abs(pt - qf), 2.5).mean()

    self.log(loss_type, loss)
    return loss

  def training_step(self, batch, batch_idx):
    return self.step_(batch, batch_idx, 'train_loss')

  def validation_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'val_loss')

  def test_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'test_loss')

  def configure_optimizers(self):
    optimizer = torch.optim.Adadelta(self.parameters(), lr=1, weight_decay=1e-10)
    scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=50, gamma=0.3)

    #optimizer = ranger.Ranger(self.parameters(), betas=(.9, 0.999), eps=1.0e-7, gc_loc=False, use_gc=False)
    #scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=50, gamma=0.3)

    return [optimizer], [scheduler]

  def flattened_parameters(self, log=False, only_weight=False):
    def join_param(joined, param):
      if log:
        print(param.size())
      joined = np.concatenate((joined, param.cpu().flatten().numpy()))
      return joined

    joined = np.array([])
    if not only_weight:
      joined = np.array([netversion]) # netversion

    print("Input layer")
    print("In :", halfka.numel())
    print("Out :", 2*BASE)
    if withFactorizer:
      # with factorizer
      joined = join_param(joined, self.white_affine.virtual_weight().t())
      print("Nb weight W: ", len(self.white_affine.virtual_weight().t()))
      if not only_weight:
        joined = join_param(joined, self.white_affine.virtual_bias())
        print("Nb bias W: ", len(self.white_affine.virtual_bias()))
        #joined = join_param(joined, self.activationIW.weight.data.t())
        #print("Nb slopes: ", len(self.activationIW.weight.data.t()))

      joined = join_param(joined, self.black_affine.virtual_weight().t())
      print("Nb weight B: ", len(self.black_affine.virtual_weight().t()))
      if not only_weight:
        joined = join_param(joined, self.black_affine.virtual_bias())
        print("Nb bias B: ", len(self.black_affine.virtual_bias()))
        #joined = join_param(joined, self.activationIB.weight.data.t())
        #print("Nb slopes: ", len(self.activationIB.weight.data.t()))
    else:
      # without factorizer
      joined = join_param(joined, self.white_affine.weight.data.t())
      print("Nb weight W: ", len(self.white_affine.weight.data.t()))
      if not only_weight:
        joined = join_param(joined, self.white_affine.bias.data)
        print("Nb bias W: ", len(self.white_affine.bias.data))
        #joined = join_param(joined, self.activationIW.weight.data.t())
        #print("Nb slopes: ", len(self.activationIW.weight.data.t()))

      joined = join_param(joined, self.black_affine.weight.data.t())
      print("Nb weight B: ", len(self.black_affine.weight.data.t()))
      if not only_weight:
        joined = join_param(joined, self.black_affine.bias.data)
        print("Nb bias B: ", len(self.black_affine.bias.data))
        #joined = join_param(joined, self.activationIB.weight.data.t())
        #print("Nb slopes: ", len(self.activationIB.weight.data.t()))

    # fc0
    print("=================")
    print("Inner layer 1")
    print("In :", 2*BASE)
    print("Out :", L1)
    for i in range(nphase):
      joined = join_param(joined, self.fc0.weight[i*L1:(i+1)*L1, :].data.t())
      print("Nb weight: ", len(self.fc0.weight[i*L1:(i+1)*L1, :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc0.bias[i*L1:(i+1)*L1].data)
        #joined = join_param(joined, self.activation0.weight[i*L1:(i+1)*L1].data.t())
        print("Nb bias: ", len(self.fc0.bias[i*L1:(i+1)*L1].data))
        #print("Nb slopes: ", len(self.activation0.weight[i*L1:(i+1)*L1].data.t()))

    # fc1
    print("=================")
    print("Inner layer 2")
    print("In :", L1)
    print("Out :", L2)
    for i in range(nphase):
      joined = join_param(joined, self.fc1.weight[i*L2:(i+1)*L2, :].data.t())
      print("Nb weight: ", len(self.fc1.weight[i*L2:(i+1)*L2, :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc1.bias[i*L2:(i+1)*L2].data)
        #joined = join_param(joined, self.activation1.weight[i*L2:(i+1)*L2].data.t())
        print("Nb bias: ", len(self.fc1.bias[i*L2:(i+1)*L2].data))
        #print("Nb slopes: ", len(self.activation1.weight[i*L2:(i+1)*L2].data.t()))

    # fc2
    print("=================")
    print("Inner layer 3")
    print("In :", L2 + L1)
    print("Out :", L3)
    for i in range(nphase):
      joined = join_param(joined, self.fc2.weight[i*L3:(i+1)*L3, :].data.t())
      print("Nb weight: ", len(self.fc2.weight[i*L3:(i+1)*L3, :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc2.bias[i*L3:(i+1)*L3].data)
        #joined = join_param(joined, self.activation2.weight[i*L3:(i+1)*L3].data.t())
        print("Nb bias: ", len(self.fc2.bias[i*L3:(i+1)*L3].data))
        #print("Nb slopes: ", len(self.activation2.weight[i*L3:(i+1)*L3].data.t()))

    # fc3
    print("=================")
    print("Output layer")
    print("In :", L3 + L2 + L1)
    print("Out :", 1)
    for i in range(nphase):
      joined = join_param(joined, self.fc3.weight[i:(i+1), :].data.t())
      print("Nb weight: ", len(self.fc3.weight[i:(i+1), :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc3.bias[i:(i+1)].data)
        #joined = join_param(joined, torch.tensor(-1))
        print("Nb bias: ", len(self.fc3.bias[i:(i+1)].data))

    print("=================")
    print(joined.shape)
    return joined.astype(np.float32)
