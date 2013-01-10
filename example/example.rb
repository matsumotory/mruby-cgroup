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

cpu.delete
io.delete
