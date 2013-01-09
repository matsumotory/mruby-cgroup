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
c = Cgroup::CPU.new("test_group")

c.rate = 50000
c.attach

while i != 10000000000
  i++
end

c.close
```

# License
under the MIT License:

* http://www.opensource.org/licenses/mit-license.php


