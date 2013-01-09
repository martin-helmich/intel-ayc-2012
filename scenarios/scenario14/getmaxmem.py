import sys

a = [int(x) for x in sys.stdin.read().strip().split(' ')]
v = []
i = 0

while i < len(a):
	v.append(a[i] + a[i+1] + a[i+2])
	i = i + 3

print max(v)
