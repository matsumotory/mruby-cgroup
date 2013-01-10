# Cgroup Module for mruby
mruby cgroup module

## install by mrbgems
 - add conf.gem line to `build_config.rb`
```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.gem :git => 'https://github.com/matsumoto-r/mruby-cgroup.git'
end
```

## example

```ruby
cpu = Cgroup::CPU.new("test_group")
io = Cgroup::BLKIO.new("test_group")

cpu.cfs_quota_us  = 30000
cpu.shares  = 2048
io.throttle_read_bps_device "8:0", "200000000"
io.throttle_write_bps_device "8:0", "100000000"
io.throttle_read_iops_device "8:0", "20000"
io.throttle_write_iops_device "8:0", "20000"
cpu.create;
io.create;

#cpu.attach
#c.loop("100000000000000000000000")
#
#cpu.attch(pid)

cpu.delete
io.delete
```

# License
under the MIT License:

* http://www.opensource.org/licenses/mit-license.php


