#! /bin/bash

no_of_pages=5
no_of_frames=3

gcc os.c -o OS
./OS $no_of_pages $no_of_frames

