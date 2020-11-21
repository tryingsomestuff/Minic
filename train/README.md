# This is the training code for building NNUE nets for Minic #

This code is vastly inspired from Seer chess engine training code and from Gary Linscott pytorch trainer for SF NNUE.

See https://github.com/glinscott/nnue-pytorch and https://github.com/connormcmonigle/seer-nnue

# Setup
```
python3 -m venv env
source env/bin/activate
pip install python-chess==0.31.4 pytorch-lightning torch
```


# Train a network

```
source env/bin/activate
python train.py train_data.bin val_data.bin
```

## Resuming from a checkpoint
```
python train.py --resume_from_checkpoint <path> ...
```

## Training on GPU
```
python train.py --gpus 1 ...
```

# Export a network

Using either a checkpoint (`.ckpt`) or serialized model (`.pt`),
you can export to SF NNUE format.  This will convert `last.ckpt`
to `nn.nnue`, which you can load directly in SF.
```
python serialize.py last.ckpt nn.nnue
```

# Import a network

Import an existing SF NNUE network to the pytorch network format.
```
python serialize.py nn.nnue converted.pt
```

# Logging

```
pip install tensorboard
tensorboard --logdir=logs
```
Then, go to http://localhost:6006/


