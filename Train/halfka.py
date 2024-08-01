import chess
import functools
import torch

def side_size():
  return (6, 8, 8)

def side_numel():
  return functools.reduce(lambda a, b: a*b, side_size())

def state_size():
  return (12, 8, 8)

def state_numel():
  return functools.reduce(lambda a, b: a*b, state_size())

def numel():
  return 64 * state_numel()

def king_idx():
  return 5

def half_ka(us, them):
  k = us[:, king_idx(), :, :].flatten(start_dim=1)
  p = torch.cat([us, them], dim=1).flatten(start_dim=1)
  result = torch.einsum('bi, bj->bij', k, p).flatten(start_dim=1)
  return result

def side_to_tensor(bd, color):
  tensor = torch.zeros(side_size(), dtype=torch.bool)
  for piece_type in range(0, 6):
    plane = torch.zeros(64, dtype=torch.bool)
    for sq in bd.pieces(piece_type+1, color):
      plane[sq] = True
    tensor[piece_type] = plane.reshape(8, 8)
  return tensor.flip(dims=(-1, ))

def to_tensors(bd):
  white = side_to_tensor(bd, color=chess.WHITE)
  black = side_to_tensor(bd, color=chess.BLACK)
  return white, black
