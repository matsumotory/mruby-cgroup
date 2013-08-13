c = Cgroup::BLKIO.new "/test"
if !c.exist?
  puts "create cgroup cpu /test"
  c.throttle_write_bps_device = "8:0 100000000"
  c.create
end

# write bps 100Mbps
puts "attach /test group with write bps 100Mbps"
c.attach
# I/O processing now
c.detach
puts "detach /test group with write bps 100Mbps"

  c.throttle_write_bps_device = "8:0 100000000"
  c.create
end

# read iops 20000iops
puts "attach /test group with read iops 20000"
c.throttle_read_iops_device = 20000
c.modify
c.attach
# I/O processing now
c.detach
puts "detach /test group with write bps 100Mbps"

if c.exist?
  puts "delete /test group"
  c.delete
end

