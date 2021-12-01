#!/bin/bash

./benchmark -db crdb -P crdb/crdb.properties -C src/test.json -E experiment_runs.txt -t -threads 100 -rows 5400000 &> results.txt
