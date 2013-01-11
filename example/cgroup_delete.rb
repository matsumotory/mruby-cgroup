c = Cgroup::CPU.new "hoge"

# hogeグループがすでにある場合は自動でload

c.delete
