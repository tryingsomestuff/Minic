import argparse
import model as M
import nnue_bin_dataset
import nnue_dataset
import pytorch_lightning as pl
import torch
from torch import set_num_threads as t_set_num_threads
from torch import set_printoptions as t_set_printoptions
from pytorch_lightning import loggers as pl_loggers
from torch.utils.data import DataLoader, Dataset

def data_loader_cc(train_filename, val_filename, num_workers, batch_size, filtered, random_fen_skipping, main_device):
  # Epoch and validation sizes are arbitrary
  epoch_size = 200000000
  val_size = 2000000
  train_infinite = nnue_dataset.SparseBatchDataset(train_filename, batch_size, num_workers=num_workers,
                                                   filtered=filtered, random_fen_skipping=random_fen_skipping, device=main_device)
  val_infinite = nnue_dataset.SparseBatchDataset(val_filename, batch_size, filtered=filtered,
                                                   random_fen_skipping=random_fen_skipping, device=main_device)
  train = DataLoader(nnue_dataset.FixedNumBatchesDataset(train_infinite, (epoch_size + batch_size - 1) // batch_size), batch_size=None, batch_sampler=None)
  val = DataLoader(nnue_dataset.FixedNumBatchesDataset(val_infinite, (val_size + batch_size - 1) // batch_size), batch_size=None, batch_sampler=None)
  return train, val

def data_loader_py(train_filename, val_filename, num_workers, batch_size):
  train = DataLoader(nnue_bin_dataset.NNUEBinData(train_filename), batch_size=batch_size, shuffle=False, num_workers=num_workers)
  val = DataLoader(nnue_bin_dataset.NNUEBinData(val_filename), batch_size=batch_size, shuffle=False)
  return train, val

def main():

  torch.set_float32_matmul_precision('medium')  # 'medium' ou 'high' pour plus de performance
  
  t_set_printoptions(profile="full")

  parser = argparse.ArgumentParser(description="Trains the network.")
  parser.add_argument("--py-data", action="store_true", help="Use python data loader (default=False)")
  parser.add_argument("train", help="Training data (.bin or .binpack)")
  parser.add_argument("val", help="Validation data (.bin or .binpack)")
  
  parser.add_argument("--accelerator", default="auto", type=str, help="Accelerator to use (auto, cpu, gpu, tpu)")
  parser.add_argument("--devices", default="auto", help="Number of devices to use")
  parser.add_argument("--max-epochs", default=400, type=int, dest='max_epochs', help="Maximum number of epochs")
  parser.add_argument("--precision", default="32-true", type=str, help="Precision (32-true, 16-mixed, bf16-mixed)")
  
  parser.add_argument("--lambda", default=1.0, type=float, dest='lambda_', help="lambda=1.0 = train on evaluations, lambda=0.0 = train on game results, interpolates between (default=1.0).")
  parser.add_argument("--uncertainty-weight", default=0.0, type=float, dest='uncertainty_weight', help="Weight of uncertainty loss: 0.0=classical MSE, 1.0=full NLL with uncertainty, 0.5=hybrid (default=0.0)")
  parser.add_argument("--num-workers", default=1, type=int, dest='num_workers', help="Number of worker threads to use for data loading. Currently only works well for binpack.")
  parser.add_argument("--batch-size", default=-1, type=int, dest='batch_size', help="Number of positions per batch / per iteration. Default on GPU = 8192 on CPU = 128.")
  parser.add_argument("--threads", default=-1, type=int, dest='threads', help="Number of torch threads to use. Default automatic (cores) .")
  parser.add_argument("--random-fen-skipping", default=0, type=int, dest='random_fen_skipping', help="skip fens randomly on average random_fen_skipping before using one.")
  parser.add_argument("--smart-fen-skipping", action='store_true', dest='smart_fen_skipping', help="If enabled positions that are bad training targets will be skipped during loading. Default: False")
  parser.add_argument("--ckpt-path", default=None, type=str, dest='ckpt_path', help="Path to checkpoint to resume training from (e.g., logs/lightning_logs/version_0/checkpoints/epoch=33.ckpt)")
  args = parser.parse_args()

  nnue = M.NNUE(lambda_=args.lambda_, uncertainty_weight=args.uncertainty_weight)

  print("Training with {} validating with {}".format(args.train, args.val))

  batch_size = args.batch_size
  if batch_size <= 0:
    batch_size = 1024 if args.accelerator == 'cpu' else 2048
  print('Using batch size {}'.format(batch_size))

  if args.threads > 0:
    print('limiting torch to {} threads.'.format(args.threads))
    t_set_num_threads(args.threads)

  tb_logger = pl_loggers.TensorBoardLogger('logs/')
  checkpoint_callback = pl.callbacks.ModelCheckpoint(save_last=True, save_top_k=-1)
  
  trainer = pl.Trainer(
    accelerator=args.accelerator,
    devices=args.devices,
    max_epochs=args.max_epochs,
    precision=args.precision,
    callbacks=[checkpoint_callback],
    logger=tb_logger,
    profiler='simple',
    gradient_clip_val=1.0  # Gradient clipping
  )

  main_device = trainer.strategy.root_device if trainer.strategy.root_device.index is None else 'cuda:' + str(trainer.strategy.root_device.index)

  if args.py_data:
    print('Using python data loader')
    train, val = data_loader_py(args.train, args.val, args.num_workers, batch_size)
  else:
    print('Using c++ data loader')
    train, val = data_loader_cc(args.train, args.val, args.num_workers, batch_size, args.smart_fen_skipping, args.random_fen_skipping, main_device)

  trainer.fit(nnue, train, val, ckpt_path=args.ckpt_path)

if __name__ == '__main__':
  main()
