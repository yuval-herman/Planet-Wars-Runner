./nob
time sudo perf record -g -F 999 ./planet_wars -config config.ini
sudo perf script -F +pid > processed.perf

