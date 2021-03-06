import os
import sys
import torch
import matplotlib.pyplot as plt

import model


def to_image(x):
  print(x.size())
  # encoding ---- king square ---- piece type ---- piece square
  x = x.reshape(8,8,  8,8,  2,6,  8,8)

  # height ---- width
  x = x.permute(0,5,6,2,  1,4,7,3)
  x = (x.reshape(4*6*8*8, 16*2*8*8)*10).abs().sigmoid()
  return x


def plot_input_weights_image(x, name, dst):
  title = f'abs(feature transformer) [{name}]'
  dst.set_title(title)
  dst.matshow(x.detach().numpy(), cmap='viridis')
  
  line_options = {'color': 'red', 'linewidth': 1}
  for i in range(1, 16):
    dst.axvline(x=2*8*8*i-1, **line_options)

  for j in range(1, 4):
    dst.axhline(y=6*8*8*j-1, **line_options)


def main():
  DPI = 150

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

  w = to_image(nnue.white_affine.weight)
  b = to_image(nnue.black_affine.weight)

  fig, (axs_w, axs_b) = plt.subplots(1, 2, figsize=(2*w.size(1) // DPI, w.size(0) // DPI), dpi=DPI)
  plot_input_weights_image(w, 'white', axs_w)
  plot_input_weights_image(b, 'black', axs_b)
  
  plt.savefig(os.path.join('./', 'feature_transformer.png'))


if __name__ == '__main__':
  main()
