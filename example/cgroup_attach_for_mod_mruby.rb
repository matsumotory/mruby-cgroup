r = Apache::Request.new
 
if r.filename == "/var/www/html/while.cgi"
  # create apache directory in /sys/fs/cgroup/cpu/ with uid apache and gid apache
  c = Cgroup::CPU.new("apache/mod_mruby_group")
  if c.exist?
    Apache.errlogger 4, "exist old cfs_quota_us: #{c.cfs_quota_us}"
    c.cfs_quota_us = 40000
    Apache.errlogger 4, "exist new cfs_quota_us: #{c.cfs_quota_us}"
    c.modify
  else
    c.cfs_quota_us = 40000
    Apache.errlogger 4, "new cfs_quota_us: #{c.cfs_quota_us}"
    c.create
  end
  c.attach
end
