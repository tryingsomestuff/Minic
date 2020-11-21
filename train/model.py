import chess
import halfka
import torch
from torch import nn
import torch.nn.functional as F
import pytorch_lightning as pl

class NNUE(pl.LightningModule):
  """
  lambda_ = 0.0 - purely based on game results
  lambda_ = 1.0 - purely based on search scores
  """
  def __init__(self, lambda_=1.0):
    super(NNUE, self).__init__()
    BASE = 256
    self.white_affine = nn.Linear(12*8*8*64, BASE)
    self.black_affine = nn.Linear(12*8*8*64, BASE)
    self.fc0 = nn.Linear(2*BASE, 32)
    self.fc1 = nn.Linear(32, 32)
    self.fc2 = nn.Linear(32, 1)
    self.lambda_ = lambda_

  def forward(self, us, them, w_in, b_in):
    w_ = self.white_affine(halfka.half_ka(w_in,b_in))
    b_ = self.black_affine(halfka.half_ka(b_in,w_in))
    base = F.relu(us * torch.cat([w_, b_], dim=1) + (1.0 - us) * torch.cat([b_, w_], dim=1))
    x = F.relu(self.fc0(base))
    x = F.relu(self.fc1(x))
    x = self.fc2(x)
    return x

  def step_(self, batch, batch_idx, loss_type):
    us, them, white, black, outcome, score = batch
  
    q = self(us, them, white, black)
    t = outcome
    # Divide score by 600.0 to match the expected NNUE scaling factor
    p = (score / 600.0).sigmoid()
    epsilon = 1e-12
    teacher_entropy = -(p * (p + epsilon).log() + (1.0 - p) * (1.0 - p + epsilon).log())
    outcome_entropy = -(t * (t + epsilon).log() + (1.0 - t) * (1.0 - t + epsilon).log())
    teacher_loss = -(p * F.logsigmoid(q) + (1.0 - p) * F.logsigmoid(-q))
    outcome_loss = -(t * F.logsigmoid(q) + (1.0 - t) * F.logsigmoid(-q))
    result  = self.lambda_ * teacher_loss    + (1.0 - self.lambda_) * outcome_loss
    entropy = self.lambda_ * teacher_entropy + (1.0 - self.lambda_) * outcome_entropy
    loss = result.mean() - entropy.mean()
    self.log(loss_type, loss)
    return loss

  def training_step(self, batch, batch_idx):
    return self.step_(batch, batch_idx, 'train_loss')

  def validation_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'val_loss')

  def test_step(self, batch, batch_idx):
    self.step_(batch, batch_idx, 'test_loss')

  def configure_optimizers(self):
    optimizer = torch.optim.Adadelta(self.parameters(), lr=1.0)
    return optimizer
