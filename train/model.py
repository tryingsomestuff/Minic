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
    BASE = 64
    self.white_affine = nn.Linear(halfka.half_ka_numel(), BASE)
    self.black_affine = nn.Linear(halfka.half_ka_numel(), BASE)
    self.fc0 = nn.Linear(2*BASE, 32)
    self.fc1 = nn.Linear(32, 16)
    self.fc2 = nn.Linear(48, 16)
    self.fc3 = nn.Linear(64, 1)
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
    x = F.relu(self.fc0(base))
    x = torch.cat([x, F.relu(self.fc1(x))], dim=1)
    x = torch.cat([x, F.relu(self.fc2(x))], dim=1)
    x = self.fc3(x)
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
    optimizer = torch.optim.Adadelta(self.parameters(), lr=1, weight_decay=1e-10)
    #scheduler = torch.optim.lr_scheduler.StepLR(optimizer, step_size=200, gamma=0.3)
    #return [optimizer], [scheduler]
    return optimizer
