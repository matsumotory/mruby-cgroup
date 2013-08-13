c = Cgroup::CPUSET.new "/test"
if !c.exist?
  puts "create cgroup cpuset /test"
  c.cpus = c.mems = "0"
  c.create
end

# CPU core 0
puts "attach /test group with cpu core 0"
c.attach
(1..100000000).each do |i| end
c.detach
puts "detach /test group with cpu core 0"

# CPU core 2
puts "attach /test group with cpu core 2"
c.cpus = c.mems = "2"
c.modify
c.attach
(1..100000000).each do |i| end
c.detach
puts "detach /test group with cpu core 2"

# CPU core 0,1,2
puts "attach /test group with cpu core 0,1,2"
c.cpus = c.mems = "0-2"
c.modify
c.attach
(1..100000000).each do |i| end
c.detach
puts "detach /test group with cpu core 0,1,2"

# CPU core 0,1,2,5
puts "attach /test group with cpu core 0,1,2,5"
c.cpus = c.mems = "0-2,5"
c.modify
c.attach
(1..100000000).each do |i| end
c.detach
puts "detach /test group with cpu core 0,1,2,5"

if c.exist?
  puts "delete /test group"
  c.delete
end

