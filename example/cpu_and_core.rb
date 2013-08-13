rate = Cgroup::CPU.new "test"
core = Cgroup::CPUSET.new "test"

rate.cfs_quota_us = 30000
core.cpus = core.mems = "0"

rate.create
core.create

# CPU core 0 and rate 30%
puts "attach /test group with cpu core 0 and rate 30%"
rate.attach
core.attach
(1..100000000).each do |i| end

# CPU core 2 and rate 50%
puts "attach /test group with cpu core 2 and rate 50%"
rate.cfs_quota_us = 50000
core.cpus = core.mems = "2"
rate.modify
core.modify
rate.attach
core.attach
(1..100000000).each do |i| end

# CPU core 0,1,2 and rate 90%
puts "attach /test group with cpu core 0,1,2 and rate 90%"
rate.cfs_quota_us = 90000
core.cpus = core.mems = "0-2"
rate.modify
core.modify
rate.attach
core.attach
(1..100000000).each do |i| end

puts "delete /test group"
rate.delete
core.delete

