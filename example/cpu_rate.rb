c = Cgroup::CPU.new("test_group")

c.rate = 20000
c.attach

c.loop("100000000000000000000000")

c.close
