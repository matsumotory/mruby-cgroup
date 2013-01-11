c = Cgroup::CPU.new "hoge"

c.shares = 2048
c.create
c.attach

# 重たい処理

