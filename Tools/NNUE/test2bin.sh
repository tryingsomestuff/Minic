#!/bin/bash
../convert/pgn-extract/pgn-extract --fencomments -Wlalg --nochecks --nomovenumbers --noresults -w500000 -N -V -o data.plain test.pgn
./Tourney/minic_dev_linux_x64 -pgn2bin data.plain

