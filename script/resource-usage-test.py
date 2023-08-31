

def some_native_calculation(n):
  result = 0
  for i in range(n):
    if i % 2 == 0:
      result += i
    else:
      result -= i
  return result

print(sum(map(some_native_calculation, range(2**12))))
