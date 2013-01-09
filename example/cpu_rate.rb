c = Cgroup::CPU.new("test_group")

c.rate = 50000
c.attach

while i != 10000000000
  i++
end

c.close

