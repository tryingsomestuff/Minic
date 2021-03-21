import argparse
import math
import model as M
import numpy
import nnue_bin_dataset
import struct
import torch
import pytorch_lightning as pl
from torch.utils.data import DataLoader

def to_binary_file(model, path):
  model.flattened_parameters().tofile(path)

def main():
  parser = argparse.ArgumentParser(description="Converts files from ckpt to nnue format.")
  parser.add_argument("source", help="Source file (can be .ckpt, .pt)")
  parser.add_argument("target", help="Target file")
  args = parser.parse_args()

  print('Converting %s to %s' % (args.source, args.target))

  if args.source.endswith(".pt") or args.source.endswith(".ckpt"):
    if args.source.endswith(".pt"):
      nnue = torch.load(args.source)
    else:
      nnue = M.NNUE.load_from_checkpoint(args.source)
    to_binary_file(nnue,args.target)
  else:
    raise Exception('Invalid filetypes: ' + str(args))

if __name__ == '__main__':
  main()
