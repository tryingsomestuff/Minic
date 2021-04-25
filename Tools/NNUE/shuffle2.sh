#!/bin/bash

split $1 -b 400M out
cat $(ls out* | sort --random-sort) >> ${1}_shuffled
