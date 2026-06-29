import chess
import halfka
import torch
import numpy as np
from torch import nn
import torch.nn.functional as F
import pytorch_lightning as pl
import struct
from collections import Counter

netversion = struct.unpack('!f', bytes.fromhex('c0ffee03'))[0]

withFactorizer = True

# CARE !! 
# change requiered in
# void fill_entry(FeatureSet<Ts...>, int i, const TrainingDataEntry& e)
# if this is changed
nbucket = 2
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


class ReduceLROnPlateauWithWarmRestarts(torch.optim.lr_scheduler.ReduceLROnPlateau):
  """
  Extends ReduceLROnPlateau with periodic warm restarts.

  After `reductions_before_restart` consecutive LR reductions the scheduler
  multiplies the current LR by `restart_factor` (capped at the LR that was
  active *before* the current reduction sequence began), then starts a fresh
  reduction cycle from there.

  Example with factor=0.5, restart_factor=2.0, reductions_before_restart=2:
    LR: 1.0 -> 0.5 -> 0.25 --(restart)--> 0.5 -> 0.25 -> 0.125 --(restart)--> 0.25 ...
  The envelope slowly drifts down while the periodic bumps keep learning alive.
  """

  def __init__(self, optimizer, mode='min', factor=0.5, patience=15,
               restart_lr=None, restart_factor=2.0, reductions_before_restart=2, **kwargs):
    """
    restart_lr: if set, jump to exactly this LR on each restart (takes priority over restart_factor).
    restart_factor: multiplicative boost applied to the post-reduction LR when restart_lr is None.
    reductions_before_restart: how many consecutive reductions trigger a restart.
    """
    if restart_lr is None and restart_factor <= 1.0:
      raise ValueError('restart_factor must be > 1.0 to actually boost the LR')
    self.restart_lr = restart_lr
    self.restart_factor = restart_factor
    self.reductions_before_restart = reductions_before_restart
    self._reduction_count = 0
    self._pre_reduction_lr = [pg['lr'] for pg in optimizer.param_groups]
    super().__init__(optimizer, mode=mode, factor=factor, patience=patience, **kwargs)

  def _target_lr(self, current_lr):
    """Return the LR to set on restart."""
    if self.restart_lr is not None:
      return self.restart_lr
    # multiplicative boost, capped at the pre-reduction reference
    return current_lr * self.restart_factor  # caller applies the cap

  def step(self, metrics):
    old_lrs = [pg['lr'] for pg in self.optimizer.param_groups]
    super().step(metrics)
    new_lrs = [pg['lr'] for pg in self.optimizer.param_groups]

    reduced = any(new < old * 0.9999 for new, old in zip(new_lrs, old_lrs))
    if reduced:
      if self._reduction_count == 0:
        # Remember LR at the start of this reduction sequence
        self._pre_reduction_lr = old_lrs
      self._reduction_count += 1

      if self._reduction_count >= self.reductions_before_restart:
        # Warm restart
        for pg, cap in zip(self.optimizer.param_groups, self._pre_reduction_lr):
          if self.restart_lr is not None:
            pg['lr'] = self.restart_lr
          else:
            pg['lr'] = min(pg['lr'] * self.restart_factor, cap)
        self._reduction_count = 0
    else:
      # No reduction this step - if we are not mid-sequence, keep updating
      # the reference so a future sequence starts from the current LR.
      if self._reduction_count == 0:
        self._pre_reduction_lr = new_lrs

  def state_dict(self):
    sd = super().state_dict()
    sd['_reduction_count'] = self._reduction_count
    sd['_pre_reduction_lr'] = self._pre_reduction_lr
    return sd

  def load_state_dict(self, state_dict):
    self._reduction_count = state_dict.pop('_reduction_count', 0)
    self._pre_reduction_lr = state_dict.pop('_pre_reduction_lr',
                                             [pg['lr'] for pg in self.optimizer.param_groups])
    super().load_state_dict(state_dict)


class NNUE(pl.LightningModule):
  """
  lambda_ = 0.0 - purely based on game results
  lambda_ = 1.0 - purely based on search scores
  """
  def __init__(self, lambda_=1.0, dropout_rate=0.05, uncertainty_weight=0.0, restart_lr=None):
    super(NNUE, self).__init__()
    funcs = [piece_position,]

    self.uncertainty_weight = uncertainty_weight
    self.restart_lr = restart_lr

    if withFactorizer:
      # with factorization
      self.white_affine = FeatureTransformer(funcs, BASE)
      self.black_affine = FeatureTransformer(funcs, BASE)
    else:
      # without factorization
      self.white_affine = nn.Linear(halfka.numel(), BASE)
      self.black_affine = nn.Linear(halfka.numel(), BASE)

    #self.d0 = nn.Dropout(p=dropout_rate * 0.5)
    self.fc0 = nn.Linear(2*BASE, L1 * nbucket)

    #self.d1 = nn.Dropout(p=dropout_rate)
    self.fc1 = nn.Linear(L1, L2 * nbucket)

    #self.d2 = nn.Dropout(p=dropout_rate)
    self.fc2 = nn.Linear(L2 + L1,  L3 * nbucket)
    
    #self.d3 = nn.Dropout(p=dropout_rate)
    self.fc3 = nn.Linear(L3 + L2 + L1,  1 * nbucket)
    self.fc3_uncertainty = nn.Linear(L3 + L2 + L1,  1 * nbucket)

    self.lambda_ = lambda_

    self.idx_offset = None

    #self.init_weights()

  def init_weights(self):
      def init_fn(m):
          if isinstance(m, nn.Linear):
              nn.init.uniform_(m.weight, -0.1, 0.1)
              if m.bias is not None:
                  nn.init.uniform_(m.bias, -0.1, 0.1)
          elif isinstance(m, FactoredBlock):
              nn.init.uniform_(m.weights, -0.1, 0.1)
          elif isinstance(m, FeatureTransformer):
              for block in m.factored_blocks:
                  init_fn(block)
              init_fn(m.affine)
      self.apply(init_fn)

  def forward(self, us, them, white, black, bucket):

    if self.idx_offset == None or self.idx_offset.shape[0] != bucket.shape[0]:
        self.idx_offset = torch.arange(0, bucket.shape[0] * nbucket, nbucket, device=bucket.device)

    indices = bucket.flatten() + self.idx_offset

    if len(white.size()) > 2: # data are from pydataloader
      w__ = halfka.half_ka(white, black)
      b__ = halfka.half_ka(black, white)
      w_ = self.white_affine(w__)
      b_ = self.black_affine(b__)
    else: # sparse data from ccdataloader
      w_ = self.white_affine(white)
      b_ = self.black_affine(black)
    
    # input layer
    base = torch.clamp(us * torch.cat([w_, b_], dim=1) + (1.0 - us) * torch.cat([b_, w_], dim=1),0,1)
    
    #if withFactorizer:
    #  base = self.d0(base)
    y0 = torch.clamp(self.fc0(base),0,1)
    y0 = y0.view(-1, L1)[indices]

    #if withFactorizer:
    #  y0 = self.d1(y0)
    y1 = torch.clamp(self.fc1(y0),0,1)
    y1 = y1.view(-1, L2)[indices]
    y1 = torch.cat([y0, y1], dim=1)

    #if withFactorizer:
    #  y1 = self.d2(y1)
    y2 = torch.clamp(self.fc2(y1),0,1)
    y2 = y2.view(-1, L3)[indices]
    y2 = torch.cat([y1, y2], dim=1)
    
    #if withFactorizer:
    #  y2 = self.d3(y2)
    y3_eval = self.fc3(y2)
    y3_eval = y3_eval.view(-1, 1)[indices]
    
    y3_log_var = self.fc3_uncertainty(y2)
    y3_log_var = y3_log_var.view(-1, 1)[indices]
    y3_var = torch.exp(torch.clamp(y3_log_var, -10, 10))

    return y3_eval, y3_var

  def step_(self, batch, batch_idx, loss_type):
    us, them, white, black, outcome, score, bucket = batch
  
    #from SF values, shall be tuned
    net2score = 600 # outFactor in the C++ code
    in_scaling = 410
    out_scaling = 360

    t = outcome

    scorenet, uncertainty = self(us, them, white, black, bucket)
    scorenet = scorenet * net2score

    # simple Minic stuff
    q = (scorenet / out_scaling).sigmoid()
    p = (score / in_scaling).sigmoid()
    pt = p * self.lambda_ + t * (1.0 - self.lambda_)
    
    # powered loss
    #loss = torch.pow(torch.abs(pt - q), 2.6).mean()
    
    # Huber loss
    #loss = F.huber_loss(q, pt, delta=0.1)

    # smaller exponent
    #loss = torch.pow(torch.abs(pt - q), 2.3).mean()

    # Combined MSE + MAE loss
    #mse_loss = F.mse_loss(q, pt)
    #mae_loss = F.l1_loss(q, pt)
    #loss = 0.7 * mse_loss + 0.3 * mae_loss
    
    # Ponderated loss
    abs_score = torch.abs(score)
    
    zone_weights = torch.where(
        abs_score < 150,
        torch.ones_like(abs_score) * 2,
        torch.where(
            abs_score < 300,
            torch.ones_like(abs_score) * 1.7,
            torch.where(
                abs_score < 500,
                torch.ones_like(abs_score) * 1.0,
                torch.where(
                    abs_score < 700,
                    torch.ones_like(abs_score) * 0.8,
                    torch.ones_like(abs_score) * 0.6
                )
            )
        )
    )

    score_expectation = (score / in_scaling).sigmoid()

    # Check if prediction direction matches outcome
    # If score > 0 and outcome = 1, or score < 0 and outcome = 0, direction is correct
    correct_direction = (score > 0) == (outcome > 0.5)

    outcome_surprise = torch.abs(outcome - score_expectation)

    # Only apply surprise penalty when direction is wrong, or reduce it when correct
    outcome_surprise = torch.where(
        correct_direction,
        outcome_surprise * 0.1,
        outcome_surprise
    )

    surprise_weight = 1.0 / (1.0 + outcome_surprise * 9.0)

    #weights = zone_weights * surprise_weight
    weights = surprise_weight
    weights = weights / weights.mean()
    
    mse = torch.pow(torch.abs(pt - q), 2.4)
    
    if self.uncertainty_weight > 0:
      uncertainty_penalty = 0.01 * torch.mean(uncertainty)
      
      uncertainty_clamped = torch.clamp(uncertainty, 0.1, 10.0)
      nll_loss = 0.5 * torch.log(2 * 3.14159 * uncertainty_clamped) + mse / (2 * uncertainty_clamped)
      
      classical_loss = mse
      loss_combined = (1.0 - self.uncertainty_weight) * classical_loss + self.uncertainty_weight * nll_loss
      loss = (weights * loss_combined).mean() + uncertainty_penalty
      
      self.log(f'{loss_type}_mean_uncertainty', uncertainty.mean())
      self.log(f'{loss_type}_std_uncertainty', uncertainty.std())
    else:
      loss = (weights * mse).mean()
    
    self.log(loss_type, loss)
    self.log(f'{loss_type}_mse', mse.mean())

    return loss

  def training_step(self, batch, batch_idx):
    loss = self.step_(batch, batch_idx, 'train_loss')
    lr = self.optimizers().param_groups[0]['lr']
    self.log('lr', lr, prog_bar=True)
    return loss

  def validation_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'val_loss')

  def test_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'test_loss')

  def configure_optimizers(self):
    
    # Strategy 1: Adadelta with lower LR and cosine annealing
    #optimizer = torch.optim.Adadelta(self.parameters(), lr=0.3, weight_decay=1e-12)
    #scheduler = torch.optim.lr_scheduler.CosineAnnealingWarmRestarts(
    #    optimizer, T_0=50, T_mult=2, eta_min=1e-6
    #)
    
    # Strategy 2: AdamW for fine-tuning
    # optimizer = torch.optim.AdamW(self.parameters(), lr=1e-3, weight_decay=1e-4)
    # scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=100, eta_min=1e-6)
    
    # Strategy 3: Adadelta with ReduceLROnPlateau + warm restarts
    # After reductions_before_restart consecutive reductions the LR is boosted
    # by restart_factor (capped at the pre-reduction level) so training can
    # escape flat regions rather than stalling at a tiny LR.
    optimizer = torch.optim.Adadelta(self.parameters(), lr=1, weight_decay=1e-13)
    scheduler = ReduceLROnPlateauWithWarmRestarts(
        optimizer, mode='min', factor=0.5, patience=15,
        restart_lr=self.restart_lr, restart_factor=2.0, reductions_before_restart=2
    )
    return {
        'optimizer': optimizer,
        'lr_scheduler': {
            'scheduler': scheduler,
            'monitor': 'val_loss',
        },
    }
    
    #return [optimizer], [scheduler]

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

      joined = join_param(joined, self.black_affine.virtual_weight().t())
      print("Nb weight B: ", len(self.black_affine.virtual_weight().t()))
      if not only_weight:
        joined = join_param(joined, self.black_affine.virtual_bias())
        print("Nb bias B: ", len(self.black_affine.virtual_bias()))
    else:
      # without factorizer
      joined = join_param(joined, self.white_affine.weight.data.t())
      print("Nb weight W: ", len(self.white_affine.weight.data.t()))
      if not only_weight:
        joined = join_param(joined, self.white_affine.bias.data)
        print("Nb bias W: ", len(self.white_affine.bias.data))

      joined = join_param(joined, self.black_affine.weight.data.t())
      print("Nb weight B: ", len(self.black_affine.weight.data.t()))
      if not only_weight:
        joined = join_param(joined, self.black_affine.bias.data)
        print("Nb bias B: ", len(self.black_affine.bias.data))

    # fc0
    print("=================")
    print("Inner layer 1")
    print("In :", 2*BASE)
    print("Out :", L1)
    for i in range(nbucket):
      joined = join_param(joined, self.fc0.weight[i*L1:(i+1)*L1, :].data.t())
      print("Nb weight: ", len(self.fc0.weight[i*L1:(i+1)*L1, :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc0.bias[i*L1:(i+1)*L1].data)
        print("Nb bias: ", len(self.fc0.bias[i*L1:(i+1)*L1].data))

    # fc1
    print("=================")
    print("Inner layer 2")
    print("In :", L1)
    print("Out :", L2)
    for i in range(nbucket):
      joined = join_param(joined, self.fc1.weight[i*L2:(i+1)*L2, :].data.t())
      print("Nb weight: ", len(self.fc1.weight[i*L2:(i+1)*L2, :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc1.bias[i*L2:(i+1)*L2].data)
        print("Nb bias: ", len(self.fc1.bias[i*L2:(i+1)*L2].data))

    # fc2
    print("=================")
    print("Inner layer 3")
    print("In :", L2 + L1)
    print("Out :", L3)
    for i in range(nbucket):
      joined = join_param(joined, self.fc2.weight[i*L3:(i+1)*L3, :].data.t())
      print("Nb weight: ", len(self.fc2.weight[i*L3:(i+1)*L3, :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc2.bias[i*L3:(i+1)*L3].data)
        print("Nb bias: ", len(self.fc2.bias[i*L3:(i+1)*L3].data))

    # fc3
    print("=================")
    print("Output layer (evaluation)")
    print("In :", L3 + L2 + L1)
    print("Out :", 1)
    for i in range(nbucket):
      joined = join_param(joined, self.fc3.weight[i:(i+1), :].data.t())
      print("Nb weight: ", len(self.fc3.weight[i:(i+1), :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc3.bias[i:(i+1)].data)
        print("Nb bias: ", len(self.fc3.bias[i:(i+1)].data))

    # fc3_uncertainty
    print("=================")
    print("Output layer (uncertainty)")
    print("In :", L3 + L2 + L1)
    print("Out :", 1)
    for i in range(nbucket):
      joined = join_param(joined, self.fc3_uncertainty.weight[i:(i+1), :].data.t())
      print("Nb weight: ", len(self.fc3_uncertainty.weight[i:(i+1), :].data.t()))
      if not only_weight:
        joined = join_param(joined, self.fc3_uncertainty.bias[i:(i+1)].data)
        print("Nb bias: ", len(self.fc3_uncertainty.bias[i:(i+1)].data))

    print("=================")
    print(joined.shape)
    return joined.astype(np.float32)
