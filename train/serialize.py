import argparse
import halfkp
import math
import model as M
import numpy
import nnue_bin_dataset
import struct
import torch
import pytorch_lightning as pl
from torch.utils.data import DataLoader

def to_binary_file(model, path):
  joined = numpy.array([])
  for p in model.parameters():
    print(p.size())
    joined = numpy.concatenate((joined, p.data.cpu().t().flatten().numpy()))
  print(joined.shape)
  joined.astype('float32').tofile(path)

def main():
  parser = argparse.ArgumentParser(description="Converts files from ckpt to nnue format.")
  parser.add_argument("source", help="Source file (can be .ckpt, .pt)")
  parser.add_argument("target", help="Target file (.nnue)")
  args = parser.parse_args()

  print('Converting %s to %s' % (args.source, args.target))

  if args.source.endswith(".pt") or args.source.endswith(".ckpt"):
    if not args.target.endswith(".nnue"):
      raise Exception("Target file must end with .nnue")
    if args.source.endswith(".pt"):
      nnue = torch.load(args.source)
    else:
      nnue = M.NNUE.load_from_checkpoint(args.source)
    to_binary_file(nnue,args.target)
  else:
    raise Exception('Invalid filetypes: ' + str(args))

if __name__ == '__main__':
  main()
