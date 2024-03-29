# This is the training code for building NNUE nets for Minic #

This code is vastly inspired/copied from Seer chess engine training code and from Gary Linscott pytorch trainer for SF NNUE.

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
more likely something like 
```
python3 train.py --log_every_n_steps 1000 --gpus 1 --batch-size 2048 --threads 4 --num-workers 4 --smart-fen-skipping --random-fen-skipping 10 ../train_data/d8_randop110_quiet.bin  ../train_data/d8_randop110_quiet.bin --resume_from_checkpoint logs/default/version_0/checkpoints/epoch=33.ckpt
```

# Logging

```
pip install tensorboard
tensorboard --logdir=logs
```
Then, go to http://localhost:6006/

# Follow training process

Launch matches between various net and a reference
```
python3 run_games.py --concurrency 3 --explore_factor 1.5 --ordo_exe /ssd/Ordo/ordo --c_chess_exe /ssd/c-chess-cli/c-chess-cli --engine /ssd/Minic/Tourney/minic_dev_linux_x64 --book_file_name /ssd/Minic/Book_and_Test/OpeningBook/8moves_last.epd logs
```

Plot the results
```
python3 plot.py -e -x -X 1.5 -p 30 -y -100 -Y 50 -m 20 -l 50 -D
```
