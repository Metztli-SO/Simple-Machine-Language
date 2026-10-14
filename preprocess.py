# preprocess.py
import sys
with open(sys.argv[1]) as f:
    lines = [l.rstrip("\n") for l in f if l.strip()]
with open("program.txt", "w") as out:
    num = 1
    for line in lines:
        out.write(f"{num} {line}\n")
        num += 1