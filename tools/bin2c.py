# tools/bin2c.py
import sys

data = open(sys.argv[1], "rb").read()

print('// a user program that calls exec("/init")')
print("// od -t xC initcode")
print("static uchar initcode[] = {")
for i in range(0, len(data), 8):
    chunk = data[i:i+8]
    print("  " + ", ".join(f"0x{x:02x}" for x in chunk) + ",")
print("};")
