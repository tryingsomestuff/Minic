import chess
import halfka
import torch
import numpy as np
from torch import nn
import torch.nn.functional as F
import pytorch_lightning as pl
import struct

netversion = struct.unpack('!f', bytes.fromhex('c0ffee00'))[0]

withFactorizer = True

def piece_position(i):
  return i % (12 * 64)

class FactoredBlock(nn.Module):
  def __init__(self, func, output_dim):
    super(FactoredBlock, self).__init__()
    self.f = torch.tensor([func(i) for i in range(halfka.half_ka_numel())], dtype=torch.long)
    self.inter_dim = 1 + self.f.max()
    self.weights = nn.Parameter(torch.zeros(self.inter_dim, output_dim))

  def virtual(self):
    with torch.no_grad():
      identity = torch.tensor([i for i in range(halfka.half_ka_numel())], dtype=torch.long)
      conversion = torch.sparse.FloatTensor(
        torch.stack([identity, self.f], dim=0),
        torch.ones(halfka.half_ka_numel()),
        size=torch.Size([halfka.half_ka_numel(), self.inter_dim])).to(self.weights.device)
      return (conversion.matmul(self.weights)).t()

  def factored(self, x):
    N, D = x.size()
    assert D == halfka.half_ka_numel()

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


class FeatureTransformer(nn.Module):
  def __init__(self, funcs, base_dim):
    super(FeatureTransformer, self).__init__()
    self.factored_blocks = nn.ModuleList([FactoredBlock(f, base_dim) for f in funcs])
    self.affine = nn.Linear(halfka.half_ka_numel(), base_dim)

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
    BASE = 128
    funcs = [piece_position,]

    if withFactorizer:
      # with factorization
      self.white_affine = FeatureTransformer(funcs, BASE)
      self.black_affine = FeatureTransformer(funcs, BASE)
    else:
      # without factorization
      self.white_affine = nn.Linear(halfka.half_ka_numel(), BASE)
      self.black_affine = nn.Linear(halfka.half_ka_numel(), BASE)

    # dropout seems necessary when using factorizer to avoid over-fitting
    self.d0 = nn.Dropout(p=0.05)
    self.fc0 = nn.Linear(2*BASE, 32)
    self.d1 = nn.Dropout(p=0.1)
    self.fc1 = nn.Linear(32, 32)
    self.d2 = nn.Dropout(p=0.1)
    self.fc2 = nn.Linear(64, 32)
    self.d3 = nn.Dropout(p=0.1)
    self.fc3 = nn.Linear(96,  1)
    self.lambda_ = lambda_

  def forward(self, us, them, white, black):

    if len(white.size()) > 2: # data are from pydataloader
      w__ = halfka.half_ka(white, black)
      b__ = halfka.half_ka(black, white)
      w_ = self.white_affine(w__)
      b_ = self.black_affine(b__)
    else: # sparse data from ccdataloader
      w_ = self.white_affine(white)
      b_ = self.black_affine(black)

    # clipped relu
    #base = torch.clamp(us * torch.cat([w_, b_], dim=1) + (1.0 - us) * torch.cat([b_, w_], dim=1),0,1)
    #x = torch.clamp(self.fc0(base),0,1)
    #x = torch.cat([x, torch.clamp(self.fc1(x),0,1)], dim=1)
    #x = torch.cat([x, torch.clamp(self.fc2(x),0,1)], dim=1)
    
    # standard relu
    base = F.relu(us * torch.cat([w_, b_], dim=1) + (1.0 - us) * torch.cat([b_, w_], dim=1))
    if withFactorizer:
      base = self.d0(base)
    x = F.relu(self.fc0(base))
    if withFactorizer:
      x = self.d1(x)
    x = torch.cat([x, F.relu(self.fc1(x))], dim=1)
    if withFactorizer:
      x = self.d2(x)
    x = torch.cat([x, F.relu(self.fc2(x))], dim=1)
    if withFactorizer:
      x = self.d3(x)
    x = self.fc3(x)
    return x

  def step_(self, batch, batch_idx, loss_type):
    us, them, white, black, outcome, score = batch
  
    net2score = 600
    in_scaling = 410
    out_scaling = 361

    q = (self(us, them, white, black) * net2score / out_scaling).sigmoid()
    t = outcome
    p = (score / in_scaling).sigmoid()
    
    #epsilon = 1e-12
    #teacher_entropy = -(p * (p + epsilon).log() + (1.0 - p) * (1.0 - p + epsilon).log())
    #outcome_entropy = -(t * (t + epsilon).log() + (1.0 - t) * (1.0 - t + epsilon).log())
    #teacher_loss = -(p * F.logsigmoid(q) + (1.0 - p) * F.logsigmoid(-q))
    #outcome_loss = -(t * F.logsigmoid(q) + (1.0 - t) * F.logsigmoid(-q))
    #result  = self.lambda_ * teacher_loss    + (1.0 - self.lambda_) * outcome_loss
    #entropy = self.lambda_ * teacher_entropy + (1.0 - self.lambda_) * outcome_entropy
    #loss = result.mean() - entropy.mean()

    loss_eval = (p - q).square().mean()
    loss_result = (p - t).square().mean()
    loss = self.lambda_ * loss_eval + (1.0 - self.lambda_) * loss_result

    self.log(loss_type, loss)
    return loss

    nnue2score = 600
    in_scaling = 410
    out_scaling = 361

    q = (self(us, them, white_indices, white_values, black_indices, black_values, psqt_indices, layer_stack_indices) * nnue2score / out_scaling).sigmoid()
    t = outcome
    p = (score / in_scaling).sigmoid()

    loss_eval = (p - q).square().mean()
    loss_result = (p - t).square().mean()
    loss = self.lambda_ * loss_eval + (1.0 - self.lambda_) * loss_result

    self.log(loss_type, loss)


  def training_step(self, batch, batch_idx):
    return self.step_(batch, batch_idx, 'train_loss')

  def validation_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'val_loss')

  def test_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'test_loss')

  def configure_optimizers(self):
    optimizer = torch.optim.Adadelta(self.parameters(), lr=1, weight_decay=1e-10)
    scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=75, gamma=0.3)
    return [optimizer], [scheduler]

  def flattened_parameters(self, log=True):
    def join_param(joined, param):
      if log:
        print(param.size())
      joined = np.concatenate((joined, param.cpu().flatten().numpy()))
      return joined
    
    joined = np.array([netversion]) # netversion

    if withFactorizer:
      # with factorizer
      joined = join_param(joined, self.white_affine.virtual_weight().t())
      joined = join_param(joined, self.white_affine.virtual_bias())
      joined = join_param(joined, self.black_affine.virtual_weight().t())
      joined = join_param(joined, self.black_affine.virtual_bias())
    else:
      # without factorizer
      joined = join_param(joined, self.white_affine.weight.data.t())
      joined = join_param(joined, self.white_affine.bias.data)
      joined = join_param(joined, self.black_affine.weight.data.t())
      joined = join_param(joined, self.black_affine.bias.data)

    # fc0
    joined = join_param(joined, self.fc0.weight.data.t())
    joined = join_param(joined, self.fc0.bias.data)
    # fc1
    joined = join_param(joined, self.fc1.weight.data.t())
    joined = join_param(joined, self.fc1.bias.data)
    # fc2
    joined = join_param(joined, self.fc2.weight.data.t())
    joined = join_param(joined, self.fc2.bias.data)
    # fc3
    joined = join_param(joined, self.fc3.weight.data.t())
    joined = join_param(joined, self.fc3.bias.data)

    print(joined.shape)
    return joined.astype(np.float32)
