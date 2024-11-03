import os
import sys
import torch
import matplotlib.pyplot as plt

import model

def to_image(x):
  print(x.size())
  # encoding ---- king square ---- piece type ---- piece square
  #              0  1   2 3   4   5 6
  x = x.reshape(24,32,  8,8,  6,  8,8)

  # height ---- width
  x = x.permute(0,2,3,4, 1,5,6)
  x = (x.reshape(24*8*8*6, 32*8*8) * 3.0).abs().sigmoid()
  return x


def plot_input_weights_image(x, name, dst):
  title = f'abs(feature transformer) [{name}]'
  cmap = 'hot'

  #dst.
  dst.set_title(title)
  dst.matshow(x, cmap=cmap)
  
  line_options = {'color': 'white', 'linewidth': 1}
  for i in range(1, 32):
    dst.axvline(x=8*8*i-1, **line_options)

  for j in range(1, 24):
    dst.axhline(y=6*8*8*j-1, **line_options)

def main():
  DPI = 300

  if len(sys.argv) < 2:
    print("Usage : python visualize.py net")
    return

  if os.path.exists(sys.argv[1]):
    print('Loading model ... ')
    nnue = model.NNUE.load_from_checkpoint(sys.argv[1])

  print('... done')

  num_total_parameters = sum(map(lambda x: torch.numel(x), nnue.parameters()))
  num_effective_parameters = len(nnue.flattened_parameters(log=False))


  print(f'total: {num_total_parameters}, effective: {num_effective_parameters}')
  nnue.cpu()
  nnue.eval()

  w = to_image(nnue.white_affine.virtual_weight())
  b = to_image(nnue.black_affine.virtual_weight())

  fig, (axs_w, axs_b) = plt.subplots(1, 2, figsize=(2*w.size(1) // DPI, w.size(0) // DPI), dpi=DPI)
  plot_input_weights_image(w, 'white', axs_w)
  plot_input_weights_image(b, 'black', axs_b)
  
  plt.savefig(os.path.join('./', 'feature_transformer.tif'))


if __name__ == '__main__':
  main()
