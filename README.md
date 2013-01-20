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
b = Cgroup::BLKIO.new("hoge")
c = Cgroup::CPU.new("fuga")

p b.exist?
if b.exist?
  p b.throttle_write_bps_device
else
  p "not found groups"
end
b.throttle_write_bps_device = "8:0", "130000000"
p b.throttle_write_bps_device
b.create

p c.exist?
if c.exist?
  p c.shares
else
  p "not found groups"
end
c.shares = 2048
p c.shares
c.create

# attach pid
c.attach 2225

# attach myself
b.attach

b.delete
c.delete
```

# License
under the MIT License:

* http://www.opensource.org/licenses/mit-license.php


