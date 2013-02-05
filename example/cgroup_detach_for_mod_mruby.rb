r = Apache::Request.new
 
if r.filename == "/var/www/html/while.cgi"
  c = Cgroup::CPU.new("apache/mod_mruby_group")
  if c.exist?
    Apache.errlogger 4, "old cfs_quota_us: #{c.cfs_quota_us}<BR>"
    c.delete
  end
end
