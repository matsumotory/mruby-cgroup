c = Cgroup::CPU.new "/test"
if !c.exist?
  puts "create cgroup cpu /test"
  c.cfs_quota_us = 30000
  c.create
end

# CPU 30%
puts "attach /test group with cpu 30%"
c.attach
(1..10000000).each do |i| end
c.detach
puts "detach /test group with cpu 30%"

# CPU 100%
puts "run with cpu 100% !!"
(1..100000000).each do |i| end

# CPU 10%
c.cfs_quota_us = 10000
c.modify
c.attach
puts "attach /test group with cpu 10%"
(1..10000000).each do |i| end
puts "detach /test group"
c.detach

if c.exist?
  puts "delete /test group"
  c.delete
end

